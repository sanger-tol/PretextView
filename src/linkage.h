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

#ifndef LINKAGE_H
#define LINKAGE_H

// Colour for a PretextGraph track whose name is a bedgraph 4th-column group.
struct linkage_colour
{
    float r, g, b, a;
};

// Returns 1 and writes the colour when name is d1..d6. NA and other names return 0.
int LinkageGroupColour(const char *name, linkage_colour *colour);

// PretextGraph stores an alg track as group ids: d1..d6 are 1..6, then other
// labels, then NA. Returns 1 for ids 1..6. Id 0 and later ids, including NA, return 0.
int LinkageGroupColourByValue(unsigned value, linkage_colour *colour);

#endif
