/* =========================================================================================

   This is an auto-generated file: Any edits you make may be overwritten!

*/

#pragma once

namespace BinaryData
{
    extern const char*   LUMINA_Manual_EN_pdf;
    const int            LUMINA_Manual_EN_pdfSize = 774670;

    extern const char*   LUMINA_Manual_JP_pdf;
    const int            LUMINA_Manual_JP_pdfSize = 836652;

    extern const char*   Screenshot1_jpg;
    const int            Screenshot1_jpgSize = 84312;

    extern const char*   Screenshot2_jpg;
    const int            Screenshot2_jpgSize = 98576;

    // Number of elements in the namedResourceList and originalFileNames arrays.
    const int namedResourceListSize = 4;

    // Points to the start of a list of resource names.
    extern const char* namedResourceList[];

    // Points to the start of a list of resource filenames.
    extern const char* originalFilenames[];

    // If you provide the name of one of the binary resource variables above, this function will
    // return the corresponding data and its size (or a null pointer if the name isn't found).
    const char* getNamedResource (const char* resourceNameUTF8, int& dataSizeInBytes);

    // If you provide the name of one of the binary resource variables above, this function will
    // return the corresponding original, non-mangled filename (or a null pointer if the name isn't found).
    const char* getNamedResourceOriginalFilename (const char* resourceNameUTF8);
}
