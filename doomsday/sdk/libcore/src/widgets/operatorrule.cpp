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

#include "de/OperatorRule"
#include "de/math.h"

namespace de {

OperatorRule::OperatorRule(Operator op, Rule const &unary)
    : Rule(), _operator(op), _leftOperand(&unary), _rightOperand(0)
{
    DENG2_ASSERT(_leftOperand != 0);

    dependsOn(_leftOperand);
}

OperatorRule::OperatorRule(Operator op, Rule const &left, Rule const &right)
    : Rule(), _operator(op), _leftOperand(&left), _rightOperand(&right)
{
    DENG2_ASSERT(_leftOperand != 0);
    DENG2_ASSERT(_rightOperand != 0);

    dependsOn(_leftOperand);
    if(_rightOperand != _leftOperand) dependsOn(_rightOperand);
}

OperatorRule::~OperatorRule()
{
    independentOf(_leftOperand);
    if(_rightOperand != _leftOperand) independentOf(_rightOperand);
}

void OperatorRule::update()
{
    float leftValue  = (_leftOperand?  _leftOperand->value()  : 0);
    float rightValue = (_rightOperand? _rightOperand->value() : 0);
    float v = leftValue;

    switch(_operator)
    {
    case Equals:
        v = leftValue;
        break;

    case Negate:
        v = -leftValue;
        break;

    case Half:
        v = leftValue / 2;
        break;

    case Double:
        v = leftValue * 2;
        break;

    case Sum:
        v = leftValue + rightValue;
        break;

    case Subtract:
        v = leftValue - rightValue;
        break;

    case Multiply:
        v = leftValue * rightValue;
        break;

    case Divide:
        v = leftValue / rightValue;
        break;

    case Maximum:
        v = de::max(leftValue, rightValue);
        break;

    case Minimum:
        v = de::min(leftValue, rightValue);
        break;

    case Floor:
        v = de::floor(leftValue);
        break;
    }

    setValue(v);
}

String OperatorRule::description() const
{
    static char const *texts[] = {
        "Equals",
        "Negate",
        "1/2x",
        "2x",
        "+",
        "-",
        "*",
        "/",
        "Max",
        "Min",
        "Floor"
    };

    String desc = "{";
    if(_leftOperand)
    {
        desc += " " + _leftOperand->description();
    }
    desc += String(" %1").arg(texts[_operator]);
    if(_rightOperand)
    {
        desc += " " + _rightOperand->description();
    }
    return desc + " }";
}

} // namespace de
