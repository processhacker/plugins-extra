#include "main.h"
//#include "zlib\zlib.h"
//#include "zlib\gzguts.h"
//
//VOID ExtractGz(VOID)
//{
//    PPH_STRING path;
//    PPH_STRING directory;
//
//    //directory = PhGetKnownLocation(CSIDL_APPDATA, L"\\Process Hacker 2\\NetExtrasPlugin\\");
//    path = PhConcatStrings2(PhGetString(directory), L"GeoLite2-Country.mmdb");
//
//    //SHCreateDirectory(NULL, PhGetString(directory));
//
//    gzFile file = gzopen("C:\\Users\\dmex\\Downloads\\GeoLite2-Country.mmdb.gz", "rb");
//    FILE* new_file = _wfopen(PhGetString(path), L"wb");
//    char buffer[PAGE_SIZE];
//
//    if (!file)
//    {
//        fprintf(stderr, "gzopen failed: %s.\n", strerror(errno));
//    }
//
//    while (!gzeof(file))
//    {
//        int bytes = gzread(file, buffer, sizeof(buffer));
//
//        // write decompressed data to disk
//        fwrite(buffer, 1, bytes, new_file);
//    }
//
//    gzclose(file);
//    fclose(new_file);
//}