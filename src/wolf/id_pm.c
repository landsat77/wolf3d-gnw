// ID_PM.C

#include "wl_def.h"

word ChunksInFile;
word PMSpriteStart;
word PMSoundStart;

bool PMSoundInfoPagePadded = false;

word *pageLengths;

byte *PMPageData;
byte **PMPages;


/*
==================
=
= PM_Startup
=
==================
*/

#ifdef GNW
/*
 * The console: the pages are used in place from the pack. tools/wf_pack.py
 * stores vswap.wl6 already laid out like the code below lays it out in RAM
 * (the sprites and the sound info page 2 byte aligned):
 *   "GWPM", u16 ChunksInFile, PMSpriteStart, PMSoundStart, sound info page
 *   padded, u32 page[ChunksInFile + 1] (offsets from the data), the data
 *   (4 byte aligned).
 */
void PM_Startup (void)
{
    char     fname[13];
    uint32_t size;
    const byte *file;
    int      i;

    snprintf (fname,sizeof(fname),"vswap.%s",extension);

    file = wf_file_data(fname,&size);
    if (!file || size < 12 || memcmp(file,"GWPM",4))
        CA_CannotOpen(fname);

    ChunksInFile = ReadShort((void *)(file + 4));
    PMSpriteStart = ReadShort((void *)(file + 6));
    PMSoundStart = ReadShort((void *)(file + 8));
    PMSoundInfoPagePadded = ReadShort((void *)(file + 10)) != 0;

    uint32_t header = 12 + 4 * (ChunksInFile + 1);
    header = (header + 3) & ~3u;
    if (size < header)
        Quit ("PM_Startup: The page file \"%s\" is damaged!",fname);
    const byte *data = file + header;

    PMPageData = NULL;
    PMPages = SafeMalloc((ChunksInFile + 1) * sizeof(*PMPages));
    pageLengths = SafeMalloc(ChunksInFile * sizeof(*pageLengths));
    for (i = 0; i <= ChunksInFile; i++)
    {
        uint32_t off = ReadLong((void *)(file + 12 + 4 * i));
        if (header + off > size)
            Quit ("PM_Startup: Illegal page offset for page %i",i);
        PMPages[i] = (byte *) data + off;
    }
    for (i = 0; i < ChunksInFile; i++)
        pageLengths[i] = (word) (PMPages[i + 1] - PMPages[i]);
}
#else
void PM_Startup (void)
{
    int      i;
    int      padding;
    byte     *page;
    uint32_t *pageOffsets;
    uint32_t pagesize;
    int32_t  filesize,datasize;
    FILE     *file;
    char     fname[13];

    snprintf (fname,sizeof(fname),"vswap.%s",extension);

    file = fopen(fname,"rb");

    if (!file)
        CA_CannotOpen(fname);

    //
    // read in header variables
    //
    fread (&ChunksInFile,sizeof(ChunksInFile),1,file);
    fread (&PMSpriteStart,sizeof(PMSpriteStart),1,file);
    fread (&PMSoundStart,sizeof(PMSoundStart),1,file);

    //
    // read in the chunk offsets
    //
    pageOffsets = SafeMalloc((ChunksInFile + 1) * sizeof(*pageOffsets));

    fread (pageOffsets,sizeof(*pageOffsets),ChunksInFile,file);

    //
    // read in the chunk lengths
    //
    pageLengths = SafeMalloc(ChunksInFile * sizeof(*pageLengths));

    fread (pageLengths,sizeof(*pageLengths),ChunksInFile,file);

    fseek (file,0,SEEK_END);
    filesize = ftell(file);
    datasize = filesize - pageOffsets[0];

    if (datasize < 0)
        Quit ("PM_Startup: The page file \"%s\" is too large!",fname);

    pageOffsets[ChunksInFile] = filesize;

    //
    // check that all chunk offsets are valid
    //
    for (i = 0; i < ChunksInFile; i++)
    {
        if (!pageOffsets[i])
            continue;           // sparse page

        if (pageOffsets[i] < pageOffsets[0] || pageOffsets[i] >= (size_t)filesize)
            Quit ("PM_Startup: Illegal page offset for page %i: %u (filesize: %u)",i,pageOffsets[i],filesize);
    }

    //
    // calculate total amount of padding needed for sprites and sound info page
    //
    padding = 0;

    for (i = PMSpriteStart; i < PMSoundStart; i++)
    {
        if (!pageOffsets[i])
            continue;           // sparse page

        if (((pageOffsets[i] - pageOffsets[0]) + padding) & 1)
            padding++;
    }

    if (((pageOffsets[ChunksInFile - 1] - pageOffsets[0]) + padding) & 1)
        padding++;

    //
    // allocate enough memory to hold the whole page file
    //
    PMPageData = SafeMalloc(datasize + padding);

    //
    // [ChunksInFile + 1] pointers to page starts
    // the last pointer points one byte after the last page
    //
    PMPages = SafeMalloc((ChunksInFile + 1) * sizeof(*PMPages));

    //
    // load pages and initialize PMPages pointers
    //
    page = &PMPageData[0];

    for (i = 0; i < ChunksInFile; i++)
    {
        if ((i >= PMSpriteStart && i < PMSoundStart) || i == ChunksInFile - 1)
        {
            //
            // pad with zeros to make it 2-byte aligned
            //
            if ((page - &PMPageData[0]) & 1)
            {
                *page++ = 0;

                if (i == ChunksInFile - 1)
                    PMSoundInfoPagePadded = true;
            }
        }

        PMPages[i] = page;

        if (!pageOffsets[i])
            continue;               // sparse page

        //
        // use specified page length when next page is sparse
        // otherwise, calculate size from the offset difference between this and the next page
        //
        if (!pageOffsets[i + 1])
            pagesize = pageLengths[i];
        else
            pagesize = pageOffsets[i + 1] - pageOffsets[i];

        fseek (file,pageOffsets[i],SEEK_SET);
        fread (page,sizeof(*page),pagesize,file);

        page += pagesize;
    }

    //
    // last page points after page buffer
    //
    PMPages[ChunksInFile] = page;

    free (pageOffsets);

    fclose (file);
}
#endif


/*
==================
=
= PM_Shutdown
=
==================
*/

void PM_Shutdown (void)
{
    free (pageLengths);
    free (PMPages);
    free (PMPageData);

    pageLengths = NULL;
    PMPages = NULL;
    PMPageData = NULL;
}


/*
==================
=
= PM_GetPageSize
=
==================
*/

uint32_t PM_GetPageSize (int page)
{
    if (page < 0 || page >= ChunksInFile)
        Quit ("PM_GetPageSize: Invalid page request: %i",page);

    return (uint32_t)(PMPages[page + 1] - PMPages[page]);
}


/*
==================
=
= PM_GetPage
=
= Returns the address of the page
=
==================
*/

byte *PM_GetPage (int page)
{
    if (page < 0 || page >= ChunksInFile)
        Quit ("PM_GetPage: Invalid page request: %i",page);

    return PMPages[page];
}


/*
==================
=
= PM_GetPageEnd
=
= Returns the address of the last page
=
==================
*/

byte *PM_GetPageEnd (void)
{
    return PMPages[ChunksInFile];
}
