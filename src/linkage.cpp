/*
Copyright (c) 2026 Genome Research Ltd.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include "linkage.h"

#include <cstring>

int
LinkageGroupColour(const char *name, linkage_colour *colour)
{
    if (!name || !colour)
    {
        return 0;
    }

    // #169e73 d1, #e59d38 d2, #1573af d3, #f0e354 d4, #60b5e1 d5, black d6.
    static const struct
    {
        const char *label;
        linkage_colour colour;
    }
    groups[] =
    {
        {"d1", {0x16 / 255.0f, 0x9e / 255.0f, 0x73 / 255.0f, 1.0f}},
        {"d2", {0xe5 / 255.0f, 0x9d / 255.0f, 0x38 / 255.0f, 1.0f}},
        {"d3", {0x15 / 255.0f, 0x73 / 255.0f, 0xaf / 255.0f, 1.0f}},
        {"d4", {0xf0 / 255.0f, 0xe3 / 255.0f, 0x54 / 255.0f, 1.0f}},
        {"d5", {0x60 / 255.0f, 0xb5 / 255.0f, 0xe1 / 255.0f, 1.0f}},
        {"d6", {0.0f, 0.0f, 0.0f, 1.0f}},
    };

    for (unsigned index = 0; index < sizeof(groups) / sizeof(groups[0]); ++index)
    {
        if (std::strcmp(name, groups[index].label) == 0)
        {
            *colour = groups[index].colour;
            return 1;
        }
    }
    return 0;
}

int
LinkageGroupColourByValue(unsigned value, linkage_colour *colour)
{
    static const char *labels[] = {"d1", "d2", "d3", "d4", "d5", "d6"};
    if (!colour || value < 1 || value > 6)
    {
        return 0;
    }
    return LinkageGroupColour(labels[value - 1], colour);
}
