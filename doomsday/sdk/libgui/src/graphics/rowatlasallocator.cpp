/** @file rowatlasallocator.cpp  Row-based atlas allocator.
 *
 * The row allocator works according to the following principles:
 *
 * - In the beginning, there is a single row that spans the height of the entire atlas.
 *   The row contains a single empty segment.
 * - If a row is completely empty, the empty space below will be split into a new empty
 *   row when the first allocation is made on the line. The first allocation also
 *   determines the initial height of the row.
 * - The height of a row may expand if there is empty space below.
 * - All the empty spaces are kept ordered from narrow to wide, so that when a new
 *   allocation is needed, the smallest suitable space can be picked.
 * - Each row is a doubly-linked list containing the used and free regions.
 * - If there are two adjacent free regions on a row, they will be merged into a larger
 *   empty space. Similarly empty rows are merged together.
 *
 * @authors Copyright (c) 2013-2014 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * LGPL: http://www.gnu.org/licenses/lgpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details. You should have received a copy of
 * the GNU Lesser General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small> 
 */

#include "de/RowAtlasAllocator"

#include <QList>
#include <set>

namespace de {

template <typename Type>
void linkAfter(Type *where, Type *object)
{
    object->next = where->next;
    object->prev = where;

    if(where->next) where->next->prev = object;
    where->next = object;
}

template <typename Type>
void unlink(Type *object)
{
    if(object->prev) object->prev->next = object->next;
    if(object->next) object->next->prev = object->prev;
    object->next = object->prev = nullptr;
}

/// The allocations are only optimized if less than 70% of the area is being utilized.
static float const OPTIMIZATION_USAGE_THRESHOLD = .7f;

DENG2_PIMPL(RowAtlasAllocator)
{
    struct Rows
    {
        struct Row;

        /**
         * Each row is composed of one or more used or empty slots.
         */
        struct Slot
        {
            Slot *next = nullptr;
            Slot *prev = nullptr;
            Row *row;

            Id id { Id::None }; ///< Id of allocation here, or Id::None if free.
            int x = 0;          ///< Left edge of the slot.
            duint width = 0;    ///< Width of the slot.
            dsize usedArea = 0;

            Slot(Row *owner) : row(owner) {}

            bool isEmpty() const
            {
                return id.isNone();
            }

            /**
             * Take an empty slot into use. The remaining empty space is split off
             * into a new slot.
             *
             * @param allocId  Allocation identifier.
             * @param widthWithMargin  Needed width.
             *
             * @return If an empty slot was created, it is returned. Otherwise @c nullptr.
             */
            Slot *allocateAndSplit(Id const &allocId, duint widthWithMargin)
            {
                DENG2_ASSERT(isEmpty());
                DENG2_ASSERT(width >= widthWithMargin);

                int const remainder = width - widthWithMargin;

                id    = allocId;
                width = widthWithMargin;

                if(remainder > 0)
                {
                    Slot *split = new Slot(row);
                    linkAfter(this, split);
                    split->x = x + width;
                    split->width = remainder;
                    return split;
                }
                return nullptr;
            }

            Slot *mergeWithNext()
            {
                DENG2_ASSERT(isEmpty());
                if(!next || !next->isEmpty()) return nullptr;

                Slot *merged = next;
                unlink(merged);
                width += merged->width;
                return merged; // Caller gets ownership.
            }

            Slot *mergeWithPrevious()
            {
                DENG2_ASSERT(isEmpty());
                if(!prev || !prev->isEmpty()) return nullptr;

                Slot *merged = prev;
                unlink(merged);
                if(row->first == merged)
                {
                    row->first = this;
                }

                x -= merged->width;
                width += merged->width;
                return merged; // Caller gets ownership.
            }

            struct SortByWidth {
                bool operator () (Slot const *a, Slot const *b) {
                    if(a->width == b->width) return a < b;
                    return a->width > b->width;
                }
            };
        };

        struct Row
        {
            Row *next = nullptr;
            Row *prev = nullptr;

            int y = 0;       ///< Top edge of the row.
            duint height = 0;
            Slot *first;    ///< There's always at least one empty slot.

            Row() : first(new Slot(this)) {}

            ~Row()
            {
                // Delete all the slots.
                Slot *next;
                for(Slot *s = first; s; s = next)
                {
                    next = s->next;
                    delete s;
                }
            }

            bool isEmpty() const
            {
                return first->isEmpty() && !first->next;
            }

            bool isTallEnough(duint heightWithMargin) const
            {
                if(height >= heightWithMargin) return true;
                // The row might be able to expand.
                if(next && next->isEmpty())
                {
                    return (height + next->height) >= heightWithMargin;
                }
                return false;
            }

            Row *split(duint newHeight)
            {
                DENG2_ASSERT(isEmpty());
                DENG2_ASSERT(newHeight <= height);

                duint const remainder = height - newHeight;
                height = newHeight;
                if(remainder > 0)
                {
                    Row *below = new Row;
                    linkAfter(this, below);
                    below->y = y + height;
                    below->height = remainder;
                    return below;
                }
                return nullptr;
            }

            void grow(duint newHeight)
            {
                DENG2_ASSERT(newHeight > height);
                DENG2_ASSERT(next);
                DENG2_ASSERT(next->isEmpty());

                duint delta = newHeight - height;
                height += delta;
                next->y += delta;
                next->height -= delta;
            }
        };

        Row *top; ///< Always at least one row exists.
        std::set<Slot *, Slot::SortByWidth> vacant; // not owned
        QHash<Id, Slot *> slotsById; // not owned

        dsize usedArea = 0; ///< Total allocated pixels.
        Instance *d;

        Rows(Instance *inst) : d(inst)
        {
            top = new Row;

            /*
             * Set up one big row, excluding the margins. This is all the space that
             * we will be using; it will be chopped up and merged back together, but
             * space will not be added or removed. Margin is reserved on the top/left
             * edge; individual slots reserve it on the right, rows reserve it in the
             * bottom.
             */
            top->y            = d->margin;
            top->height       = d->size.y - d->margin;
            top->first->x     = d->margin;
            top->first->width = d->size.x - d->margin;

            addVacant(top->first);
        }

        ~Rows()
        {
            Row *next;
            for(Row *r = top; r; r = next)
            {
                next = r->next;
                delete r;
            }
        }

        void addVacant(Slot *slot)
        {
            DENG2_ASSERT(slot->isEmpty());            
            vacant.insert(slot);            
            DENG2_ASSERT(*vacant.find(slot) == slot);
        }

        void removeVacant(Slot *slot)
        {
            DENG2_ASSERT(vacant.find(slot) != vacant.end());
            vacant.erase(slot);            
            DENG2_ASSERT(vacant.find(slot) == vacant.end());
        }

        Slot *findBestVacancy(Atlas::Size const &size) const
        {
            Slot *best = nullptr;

            // Look through the vacancies starting with the widest one. Statistically
            // there are more narrow empty slots than wide ones.
            for(Slot *s : vacant)
            {
                if(s->width >= size.x + d->margin)
                {
                    if(s->row->isTallEnough(size.y + d->margin))
                    {
                        best = s;
                    }
                }
                else
                {
                    // Too narrow, the rest is also too narrow.
                    break;
                }
            }

            return best;
        }

        /**
         * Allocate a slot for the specified size. The area used by the slot may be
         * larger than the requested size.
         *
         * @param size  Dimensions of area to allocate.
         * @param rect  Allocated rectangle is returned here.
         * @param id    Id for the new slot.
         *
         * @return Allocated slot, or @c nullptr.
         */
        Slot *alloc(Atlas::Size const &size, Rectanglei &rect, Id::Type id = Id::None)
        {
            Slot *slot = findBestVacancy(size);
            if(!slot) return nullptr;

            DENG2_ASSERT(slot->isEmpty());

            // This slot will be taken into use.
            removeVacant(slot);

            Atlas::Size const needed = size + Atlas::Size(d->margin, d->margin);

            // The first allocation determines the initial row height. The remainder
            // is split into a new empty row (if something remains).
            if(slot->row->isEmpty())
            {
                if(Row *addedRow = slot->row->split(needed.y))
                {
                    // Give this new row the correct width.
                    addedRow->first->x = d->margin;
                    addedRow->first->width = d->size.x - d->margin;

                    addVacant(addedRow->first);
                }
            }

            // The row may expand if needed.
            if(slot->row->height < needed.y)
            {
                slot->row->grow(needed.y);
            }

            // Got a place, mark it down.
            if(Slot *addedSlot = slot->allocateAndSplit(id? Id(id) : Id(), needed.x))
            {
                addVacant(addedSlot);
            }
            slotsById.insert(slot->id, slot);
            rect = Rectanglei::fromSize(Vector2i(slot->x, slot->row->y), size);
            slot->usedArea = size.x * size.y;
            usedArea += slot->usedArea;

            DENG2_ASSERT(usedArea <= d->size.x * d->size.y);
            DENG2_ASSERT(vacant.find(slot) == vacant.end());
            DENG2_ASSERT(!slot->isEmpty());

            return slot;
        }

        void mergeLeft(Slot *slot)
        {
            if(Slot *removed = slot->mergeWithPrevious())
            {
                removeVacant(removed);
                delete removed;
            }
        }

        void mergeRight(Slot *slot)
        {
            if(Slot *removed = slot->mergeWithNext())
            {
                removeVacant(removed);
                delete removed;
            }
        }

        void mergeAbove(Row *row)
        {
            DENG2_ASSERT(row->isEmpty());
            if(row->prev && row->prev->isEmpty())
            {
                Row *merged = row->prev;
                unlink(merged);
                if(top == merged)
                {
                    top = row;
                }
                row->y -= merged->height;
                row->height += merged->height;

                removeVacant(merged->first);
                delete merged;
            }
        }

        void mergeBelow(Row *row)
        {
            DENG2_ASSERT(row->isEmpty());
            if(row->next && row->next->isEmpty())
            {
                Row *merged = row->next;
                unlink(merged);
                row->height += merged->height;

                removeVacant(merged->first);
                delete merged;
            }
        }

        void release(Id const &id)
        {
            DENG2_ASSERT(slotsById.contains(id));

            // Make the slot vacant again.
            Slot *slot = slotsById.take(id);
            slot->id = Id::None;

            DENG2_ASSERT(slot->usedArea > 0);
            DENG2_ASSERT(usedArea >= slot->usedArea);

            usedArea -= slot->usedArea;

            mergeLeft(slot);
            mergeRight(slot);

            addVacant(slot);

            // Empty rows will merge together.
            if(slot->row->isEmpty())
            {
                mergeAbove(slot->row);
                mergeBelow(slot->row);
            }
        }
    };

    Atlas::Size size;
    int margin { 0 };
    Allocations allocs;
    std::unique_ptr<Rows> rows;

    Instance(Public *i)
        : Base(i)
        , rows(new Rows(this))
    {}

    struct ContentSize {
        Id::Type id;
        Atlas::Size size;

        ContentSize(Id const &allocId, Vector2ui const &sz) : id(allocId), size(sz) {}
        bool operator < (ContentSize const &other) const {
            if(size.y == other.size.y) {
                // Secondary sorting by descending width.
                return size.x > other.size.x;
            }
            return size.y > other.size.y;
        }
    };

    bool optimize()
    {
        // Set up a LUT based on descending allocation width.
        QList<ContentSize> descending;
        DENG2_FOR_EACH(Allocations, i, allocs)
        {
            descending.append(ContentSize(i.key(), i.value().size()));
        }
        qSort(descending);

        Allocations optimal;
        std::unique_ptr<Rows> revised(new Rows(this));

        for(auto const &ct : descending)
        {
            Rectanglei optRect;
            if(!revised->alloc(ct.size, optRect, ct.id))
            {
                return false; // Ugh, can't actually fit these.
            }
            optimal[ct.id] = optRect;
        }

        allocs = optimal;
        rows.reset(revised.release());
        return true;
    }

    float usage() const
    {
        return float(rows->usedArea) / float(size.x * size.y);
    }
};

RowAtlasAllocator::RowAtlasAllocator() : d(new Instance(this))
{}

void RowAtlasAllocator::setMetrics(Atlas::Size const &totalSize, int margin)
{
    d->size   = totalSize;
    d->margin = margin;

    DENG2_ASSERT(d->allocs.isEmpty());
    d->rows.reset(new Instance::Rows(d));
}

void RowAtlasAllocator::clear()
{
    d->rows.reset(new Instance::Rows(d));
    d->allocs.clear();
}

Id RowAtlasAllocator::allocate(Atlas::Size const &size, Rectanglei &rect)
{
    if(auto *slot = d->rows->alloc(size, rect))
    {
        d->allocs[slot->id] = rect;
        return slot->id;
    }

    // Couldn't find a suitable place.
    return 0;
}

void RowAtlasAllocator::release(Id const &id)
{
    DENG2_ASSERT(d->allocs.contains(id));

    d->rows->release(id);
    d->allocs.remove(id);
}

int RowAtlasAllocator::count() const
{
    return d->allocs.size();
}

Atlas::Ids RowAtlasAllocator::ids() const
{
    Atlas::Ids ids;
    foreach(Id const &id, d->allocs.keys())
    {
        ids.insert(id);
    }
    return ids;
}

void RowAtlasAllocator::rect(Id const &id, Rectanglei &rect) const
{
    DENG2_ASSERT(d->allocs.contains(id));
    rect = d->allocs[id];
}

RowAtlasAllocator::Allocations RowAtlasAllocator::allocs() const
{
    return d->allocs;
}

bool RowAtlasAllocator::optimize()
{
    // Optimization is not attempted unless there is a significant portion of
    // unused space.
    if(d->usage() >= OPTIMIZATION_USAGE_THRESHOLD) return false;

    return d->optimize();
}

} // namespace de
