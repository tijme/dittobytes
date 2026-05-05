/**
 * GNU General Public License, version 2.0.
 *
 * Copyright (c) 2025 Tijme Gommers (@tijme).
 *
 * This source code file is part of Dittobytes. Dittobytes is 
 * licensed under GNU General Public License, version 2.0, and 
 * you are free to use, modify, and distribute this file under 
 * its terms. However, any modified versions of this file must 
 * include this same license and copyright notice.
 */

/**
 * Integers.
 * 
 * Defines macros that specify limits of integer types corresponding to types defined in other standard headers.
 * https://pubs.opengroup.org/onlinepubs/009696899/basedefs/stdint.h.html
 */
#include <stdint.h>

/**
 * A simple class demonstrating the use of private and public variables.
 * 
 * Please do note that you cannot use external libraries by default. This means you cannot 
 * make use of e.g. `std::string`, as it's part of `libstdc++` or `libc++`.
 */
class ExampleClass {

private:
    char SomeString[4] = "ABC";
    uint32_t NumberOne = 1;

public:
    uint32_t NumberTwo = 2;
  
    /**
     * Return the sum of different variable values in this class.
     * 
     * @return uint32_t The sum of different variable values in this class.
     */
    uint32_t TestGlobalsInClass() {
        return NumberOne + NumberTwo + static_cast<uint32_t>(SomeString[1]);
    }

};

/**
 * The main function of your shellcode. In this example we solely return a value.
 * 
 * @return uint64_t A return value of your shellcode.
 */
uint64_t EntryFunction() {
    ExampleClass example;
    return (uint64_t) example.TestGlobalsInClass();
}
