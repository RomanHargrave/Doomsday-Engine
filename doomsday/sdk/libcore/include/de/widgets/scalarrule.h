/*
 * The Doomsday Engine Project
 *
 * Copyright © 2011-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDENG2_SCALARRULE_H
#define LIBDENG2_SCALARRULE_H

#include "rule.h"

#include "../Animation"
#include "../Time"

namespace de {

/**
 * Rule with a scalar value. The value is animated over time.
 * @ingroup widgets
 */
class DENG2_PUBLIC ScalarRule : public Rule, DENG2_OBSERVES(Clock, TimeChange)
{
public:
    explicit ScalarRule(float initialValue);

    void set(float target, TimeDelta transition = 0, TimeDelta delay = 0);

    void set(Rule const &target, TimeDelta transition = 0, TimeDelta delay = 0);

    /**
     * Sets the animation style of the rule.
     *
     * @param style  Animation style.
     */
    void setStyle(Animation::Style style);

    void setStyle(Animation::Style style, float bounceSpring);

    /**
     * Read-only access to the scalar animation.
     */
    Animation const &animation() const {
        return _animation;
    }

    /**
     * Shifts the scalar rule's animation target and current value without
     * affecting any ongoing animation.
     *
     * @param delta  Value delta for the shift.
     */
    void shift(float delta);

    void finish();

    void pause();

    void resume();

    String description() const;

protected:
    ~ScalarRule();
    void update();

    void timeChanged(Clock const &);

private:
    Animation _animation;
    Rule const *_targetRule;
};

} // namespace de

#endif // LIBDENG2_SCALARRULE_H
