/** @file childwidgetorganizer.cpp Organizes widgets according to a UI context.
 *
 * @authors Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/ChildWidgetOrganizer"
#include "de/LabelWidget"
#include "de/ui/Item"

#include <de/App>
#include <QMap>

namespace de {

static DefaultWidgetFactory defaultWidgetFactory;

DENG2_PIMPL(ChildWidgetOrganizer),
DENG2_OBSERVES(Widget,   Deletion   ),
DENG2_OBSERVES(ui::Data, Addition   ),
DENG2_OBSERVES(ui::Data, Removal    ),
DENG2_OBSERVES(ui::Data, OrderChange),
DENG2_OBSERVES(ui::Item, Change     )
{
    GuiWidget *container;
    ui::Data const *context;
    IWidgetFactory *factory;
    IFilter const *filter;

    typedef QMap<ui::Item const *, GuiWidget *> Mapping;
    typedef QMutableMapIterator<ui::Item const *, GuiWidget *> MutableMappingIterator;
    Mapping mapping; ///< Maps items to corresponding widgets.

    Instance(Public *i, GuiWidget *c)
        : Base(i),
          container(c),
          context(0),
          factory(&defaultWidgetFactory),
          filter(0)
    {}

    ~Instance()
    {
        DENG2_FOR_EACH_CONST(Mapping, i, mapping)
        {
            i.value()->audienceForDeletion() -= this;
        }
    }

    void set(ui::Data const *ctx)
    {
        if(context)
        {
            context->audienceForAddition() -= this;
            context->audienceForRemoval() -= this;
            context->audienceForOrderChange() -= this;

            clearWidgets();
            context = 0;
        }

        context = ctx;

        if(context)
        {
            makeWidgets();

            context->audienceForAddition() += this;
            context->audienceForRemoval() += this;
            context->audienceForOrderChange() += this;
        }
    }

    void addItemWidget(ui::Data::Pos pos, bool alwaysAppend = false)
    {
        DENG2_ASSERT_IN_MAIN_THREAD(); // widgets should only be manipulated in UI thread
        DENG2_ASSERT(factory != 0);

        if(filter)
        {
            if(!filter->isItemAccepted(self, *context, pos))
            {
                // Skip this one.
                return;
            }
        }

        ui::Item const &item = context->at(pos);
        GuiWidget *w = factory->makeItemWidget(item, container);
        if(!w) return; // Unpresentable.

        // Update the widget immediately.
        mapping.insert(&item, w);
        itemChanged(item);

        if(alwaysAppend || pos == context->size() - 1)
        {
            // This is the last item.
            container->add(w);
        }
        else
        {
            container->insertBefore(w, *mapping[&context->at(pos + 1)]);
        }

        // Others may alter the widget in some way.
        DENG2_FOR_PUBLIC_AUDIENCE2(WidgetCreation, i)
        {
            i->widgetCreatedForItem(*w, item);
        }

        // Observe.
        w->audienceForDeletion() += this; // in case it's manually deleted
        item.audienceForChange() += this;
    }

    void makeWidgets()
    {
        DENG2_ASSERT(context != 0);
        DENG2_ASSERT(container != 0);

        for(ui::Data::Pos i = 0; i < context->size(); ++i)
        {
            addItemWidget(i, true /*always append*/);
        }
    }

    void deleteWidget(GuiWidget *w)
    {
        w->audienceForDeletion() -= this;
        GuiWidget::destroy(w);
    }

    void clearWidgets()
    {
        DENG2_FOR_EACH_CONST(Mapping, i, mapping)
        {
            i.key()->audienceForChange() -= this;

            deleteWidget(i.value());
        }
        mapping.clear();
    }

    void widgetBeingDeleted(Widget &widget)
    {
        /*
         * Note: this should not occur normally, as the widgets created by the
         * Organizer are not usually manually deleted.
         */
        MutableMappingIterator iter(mapping);
        while(iter.hasNext())
        {
            iter.next();
            if(iter.value() == &widget)
            {
                iter.remove();
                break;
            }
        }
    }

    void dataItemAdded(ui::Data::Pos pos, ui::Item const &)
    {
        addItemWidget(pos);
    }

    void dataItemRemoved(ui::Data::Pos, ui::Item &item)
    {
        Mapping::iterator found = mapping.find(&item);
        if(found != mapping.constEnd())
        {
            found.key()->audienceForChange() -= this;
            deleteWidget(found.value());
            mapping.erase(found);
        }
    }

    void dataItemOrderChanged()
    {
        // Remove all widgets and put them back in the correct order.
        DENG2_FOR_EACH_CONST(Mapping, i, mapping)
        {
            container->remove(*i.value());
        }
        for(ui::Data::Pos i = 0; i < context->size(); ++i)
        {
            if(mapping.contains(&context->at(i)))
            {
                container->add(mapping[&context->at(i)]);
            }
        }
    }

    void itemChanged(ui::Item const &item)
    {
        if(!mapping.contains(&item))
        {
            // Not represented as a child widget.
            return;
        }

        GuiWidget &w = *mapping[&item];
        factory->updateItemWidget(w, item);

        // Notify.
        DENG2_FOR_PUBLIC_AUDIENCE2(WidgetUpdate, i)
        {
            i->widgetUpdatedForItem(w, item);
        }
    }

    GuiWidget *find(ui::Item const &item) const
    {
        Mapping::const_iterator found = mapping.constFind(&item);
        if(found == mapping.constEnd()) return 0;
        return found.value();
    }

    GuiWidget *findByLabel(String const &label) const
    {
        DENG2_FOR_EACH_CONST(Mapping, i, mapping)
        {
            if(i.key()->label() == label)
            {
                return i.value();
            }
        }
        return 0;
    }

    ui::Item const *findByWidget(GuiWidget const &widget) const
    {
        DENG2_FOR_EACH_CONST(Mapping, i, mapping)
        {
            if(i.value() == &widget)
            {
                return i.key();
            }
        }
        return 0;
    }

    DENG2_PIMPL_AUDIENCE(WidgetCreation)
    DENG2_PIMPL_AUDIENCE(WidgetUpdate)
};

DENG2_AUDIENCE_METHOD(ChildWidgetOrganizer, WidgetCreation)
DENG2_AUDIENCE_METHOD(ChildWidgetOrganizer, WidgetUpdate)

ChildWidgetOrganizer::ChildWidgetOrganizer(GuiWidget &container)
    : d(new Instance(this, &container))
{}

void ChildWidgetOrganizer::setContext(ui::Data const &context)
{
    d->set(&context);
}

void ChildWidgetOrganizer::unsetContext()
{
    d->set(0);
}

ui::Data const &ChildWidgetOrganizer::context() const
{
    DENG2_ASSERT(d->context != 0);
    return *d->context;
}

GuiWidget *ChildWidgetOrganizer::itemWidget(ui::Data::Pos pos) const
{
    return itemWidget(context().at(pos));
}

void ChildWidgetOrganizer::setWidgetFactory(IWidgetFactory &factory)
{
    d->factory = &factory;
}

ChildWidgetOrganizer::IWidgetFactory &ChildWidgetOrganizer::widgetFactory() const
{
    DENG2_ASSERT(d->factory != 0);
    return *d->factory;
}

void ChildWidgetOrganizer::setFilter(ChildWidgetOrganizer::IFilter const &filter)
{
    d->filter = &filter;
}

GuiWidget *ChildWidgetOrganizer::itemWidget(ui::Item const &item) const
{
    return d->find(item);
}

GuiWidget *ChildWidgetOrganizer::itemWidget(String const &label) const
{
    return d->findByLabel(label);
}

ui::Item const *ChildWidgetOrganizer::findItemForWidget(GuiWidget const &widget) const
{
    return d->findByWidget(widget);
}

GuiWidget *DefaultWidgetFactory::makeItemWidget(ui::Item const &, GuiWidget const *)
{
    return new LabelWidget;
}

void DefaultWidgetFactory::updateItemWidget(GuiWidget &widget, ui::Item const &item)
{
    widget.as<LabelWidget>().setText(item.label());
}

} // namespace de
