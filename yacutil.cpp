// Yacoin Utility console module
// Reads yacoin blockchain file, and file with sorted hash values, writes filtered and sorted blocks down
//
// block "0000060fc90618113cde415ead019a1052a9abc43afcccff38608ff8751353e5" with height 0 (getblockhash 0)
// references inital (hardcoded) block "0000000000000000000000000000000000000000000000000000000000000000"
//
// in order to dump blocks from 101 .. 200 you need sorted hashes from 100 to 201 (NOT 199) and limit from (101,200)
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#include <wx/app.h>
#include <wx/filename.h>
#include <wx/filefn.h>
#include <wx/cmdline.h>
#include <wx/arrstr.h>
#include <wx/wfstream.h>
#include <wx/datstrm.h>
#include <wx/hashmap.h>
#include <wx/log.h>
#include "scrypt-mine.h"
//#include "uint256.h"


#if defined __WINDOWS__
#define wxEOL wxString("\r\n" )
#elif defined __LINUX__
#define wxEOL wxString("\n" )
#elif defined __APPLE__
#define wxEOL wxString("\r" )
#endif


#define LOG_INIT wxLogStream logger; wxLog::SetActiveTarget(&logger); logger.SetFormatter(new xLogFormatter()); //wxLog::EnableLogging(false); // disable?
#define LOG_PREV_BLOCK_HASH //wxPuts(wxString("LPBH ").Append(prevBlockHashString));
#define LOG_PREV_BLOCK_MULTIPLE //wxPuts(wxString("LPBM ").Append(it->first));
#define LOG_SORTED_HASH_READ //wxPuts(wxString("LSHR ").Append(n));
#define LOG_DUMP_FIRST_BLOCK //wxPuts("DUMP block 0");
#define LOG_HASH_WITHOUT_PARENT //wxPuts(wxString("No parent: ").Append(prevBlockHashString));
#define LOG_PREV_HASH_IN_SORTLIST //wxPuts("-");
#define LOG_HASH_JANE //wxPuts(wxString("LHJ hcnt>0   does hash exists: ").Append(wxString::Format("%i",dist)));
#define LOG_DUMP_JANE //wxPuts(wxString("LDJ DUMP sibling at: ").Append(wxString::Format("%i",wBlksCnt)));
#define LOG_HASH_ONLY_SON //wxLogMessage(wxString("The Only Son distance: ").Append(wxString::Format("%i",dist)));
#define LOG_DUMP_ONLY_SON //wxLogMessage(wxString("DUMP only son at: ").Append(wxString::Format("%i",wBlksCnt)));
#define LOG_SIBLING_TO_MAP //wxPuts( wxString("LSTM SIBLING -> locMap;  size: ").Append(wxString::Format("%i",locMap.size())) );
#define LOG_ONLY_SON_TO_MAP //wxPuts( wxString("LOSTM ONLY SON -> locMap; size: ").Append(wxString::Format("%i",locMap.size())) );
#define LOG_CHECK_MAP //wxLogMessage("Cache hash map Check ");
#define LOG_DUMP_FROM_MAP //wxLogMessage (wxString("DUMP from MAP at file loc: ").Append(wxString::Format("%i",locMapKey)));
#define LOG_HASH_NOT_IN_MAP //wxPuts("Unknown block or maybe last sibling without parent?");
#define LOG_DUMP_SKIPPED //wxPuts("--- SKIPPED previous DUMP");



static const wxCmdLineEntryDesc g_cmdLineDesc [] =
{
       { wxCMD_LINE_SWITCH, "h", "help" , "displays help", wxCMD_LINE_VAL_NONE, wxCMD_LINE_OPTION_HELP },
       { wxCMD_LINE_OPTION, "s", "skip" , "skip reading <str> bytes of blockchain file" },
       { wxCMD_LINE_OPTION, "t", "terminate" , "terminate blockchain file scan after <str> blocks read" },
       { wxCMD_LINE_OPTION, "b", "begin" , "index of starting sorted block hash" },
       { wxCMD_LINE_OPTION, "e", "end" , "index of ending sorted block hash" },
       { wxCMD_LINE_PARAM, NULL, NULL, "params", wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_OPTIONAL|wxCMD_LINE_PARAM_MULTIPLE  },
       { wxCMD_LINE_NONE }
};

void AssignIfNumberGt0(wxInt32& pTargetNumber, wxString& pTestData)
{
    long longnum;

    if ( pTestData.ToLong(&longnum) )
    {
        if ( longnum > 0 )
        {
            pTargetNumber = (long int) longnum;
        }
    }
}

bool ProcessCmdLine (wxCmdLineParser& parser, wxArrayString& pStringArgs, wxInt32& pMin, wxInt32& pMax, wxInt32& pSkipPercent, wxFileOffset& pSkipBytes, wxInt32& pScanBlockTreshold)
{
    parser.SetDesc(g_cmdLineDesc);
    //parser.DisableLongOptions();
    parser.SetLogo( "##################################" + wxEOL +
                    "# Yacoin Utility v0.1 #" + wxEOL +
                    "##################################" + wxEOL + wxEOL +
                    "Reads two input files: " + wxEOL + "blockchain *.dat file and file with sorted block hashes" + wxEOL +
                    "Then writes blocks from first file sorted " + wxEOL +
                    "according to entries in second file to output file" + wxEOL + wxEOL);

    parser.AddUsageText(wxEOL + "yacutil \"infile.dat\" \"sort.lst\" \"sorted.out\" 90001 90100 3% -t 100000");

    pMin = pMax = -1;
    pSkipPercent = 0;
    wxString tmp;

    switch (parser.Parse(true))
    {
        case -1: // --help /h
            return false;

        case 0:
            if ( parser.GetParamCount() == 0 )
            {
                return false;
            }

            long longnum;

            // first process parameters without options
            for (size_t i=0; i < parser.GetParamCount(); i++)
            {
                wxString str = parser.GetParam(i);

                if ( str.ToLong(&longnum) )
                {
                    wxInt32 intnum = (long int) longnum;

                    if (pMin < 0)
                    {
                        pMin = intnum;
                    }

                    if ( pMin > intnum )
                    {
                        if (intnum == 0)
                        {
                            pMax = intnum;
                        }
                        else
                        {
                            pMax = pMin;
                            pMin = intnum;
                        }
                    }
                    else
                    {
                        pMax = intnum;
                    }
                }
                else
                {
                    tmp = str;
                    tmp.Replace("%",wxEmptyString);

                    if ( tmp.ToLong(&longnum) )
                    {
                        if ( longnum > 0 && longnum < 100 )
                        {
                            pSkipPercent = (unsigned int) longnum;
                        }
                    }
                    else
                    {
                        pStringArgs.Add(tmp);
                    }
                }

            }

            // options override parameters above

            if (parser.Found( "s" , &tmp))  // skip bytes
            {
                if ( tmp.ToLong(&longnum) )
                {
                    if ( longnum > 0 )
                    {
                        pSkipBytes = (long int) longnum;
                    }
                }
            };

            if (parser.Found( "t" , &tmp))  // terminate after t blocks read
            {
                AssignIfNumberGt0(pScanBlockTreshold,tmp);
            };

            if (parser.Found( "b" , &tmp))  // begin with sorted hash index b
            {
                AssignIfNumberGt0(pMin,tmp);
            };

            if (parser.Found( "e" , &tmp))  // end with sorted hash index e
            {
                AssignIfNumberGt0(pMax,tmp);
            };

            break;

        default:
            return false;
    }

    if ( pMax == -1 )
    {
        pMin=1;
        pMax=0;
    }

    return true ;
}



typedef struct
{
    wxUint32 version;
    unsigned char prev_block[32];
    unsigned char merkle_root[32];
    wxUint32 timestamp;
    wxUint32 bits;
    wxUint32 nonce;

} block_header;

typedef struct
{
    wxUint32 dist;
    wxUint32 bytesLeft;

} BlockFileLocType;

WX_DECLARE_HASH_MAP( wxUint32,
                     BlockFileLocType,
                     wxIntegerHash,
                     wxIntegerEqual,
                     BlockLocMapType ); // map: key=fileOffset, value={block distance from current height, bytesLeft}

WX_DECLARE_HASH_MAP( wxString, //std::string, // char*, // uint256, // KEY_T,      // type of the keys
                     wxUint32, //VALUE_T,    // type of the values
                     wxStringHash , // HASH_T,     // hasher
                     wxStringEqual, // KEY_EQ_T,   // key equality predicate
                     hashMapType // CLASSNAME  // name of the class
                    ); // map: key=block hash, value = int; hmap["0000060fc90618113cde415ead019a1052a9abc43afcccff38608ff8751353e5"] = 1

wxUint32 g_magic = -437786919;

unsigned char g_block0 [] = {
 0xd9,0xe6,0xe7,0xe5,0xc9,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00
,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x4f,0x24,0xf2,0x4c
,0x9d,0xf3,0x25,0xe9,0xdd,0x69,0xcc,0xb7,0x21,0x80,0x6d,0xb2,0xf7,0xd7,0x57,0x9d
,0xfa,0xd3,0x91,0xa5,0x76,0x66,0xf0,0x9f,0x41,0x76,0x8b,0x67,0xb4,0xe3,0x89,0x51
,0xff,0xff,0x0f,0x1e,0x7d,0xf1,0x01,0x00,0x01,0x01,0x00,0x00,0x00,0xa0,0xe3,0x89
,0x51,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
,0x00,0x00,0xff,0xff,0xff,0xff,0x37,0x04,0xff,0xff,0x00,0x1d,0x02,0x0f,0x27,0x2e
,0x68,0x74,0x74,0x70,0x73,0x3a,0x2f,0x2f,0x62,0x69,0x74,0x63,0x6f,0x69,0x6e,0x74
,0x61,0x6c,0x6b,0x2e,0x6f,0x72,0x67,0x2f,0x69,0x6e,0x64,0x65,0x78,0x2e,0x70,0x68
,0x70,0x3f,0x74,0x6f,0x70,0x69,0x63,0x3d,0x31,0x39,0x36,0x31,0x39,0x36,0xff,0xff
,0xff,0xff,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
,0x00 };

wxUint32 g_targetDumpCount = 0;



class SortedBlockWriter
{
    wxFFileOutputStream* m_fos;
    wxDataOutputStream* m_dos;
    wxFFile m_file;
    wxUint32 m_dumped;
    unsigned char* m_buffer;
public:

    SortedBlockWriter() : m_fos(NULL), m_dos(NULL) {}

    SortedBlockWriter(wxString pFileName)
    {
        m_buffer = new unsigned char[1048576];
        OpenFile(pFileName);
    }

    bool OpenFile (wxString pFileName)
    {
        m_dumped = 0;

        if ( m_file.Open (pFileName,"wb") )
        {
            m_fos = new wxFFileOutputStream( m_file );
            m_dos = new wxDataOutputStream( *m_fos );
        }
        return m_file.IsOpened();
    }

    unsigned char* GetBuffer()
    {
        return m_buffer;
    }

    bool dump0()    // dump block 0
    {
        m_fos->WriteAll(g_block0,209);
        m_dumped++;
        return true;
    }

    bool dump(wxUint32& pLen) // dump bytes array
    {
        if (g_targetDumpCount > m_dumped)
        {
            m_dos->Write32(&g_magic,1);
            m_dos->Write32(&pLen,1);
            m_fos->WriteAll(m_buffer,pLen);
            m_dumped++;
        }
        else
        {
            LOG_DUMP_SKIPPED // wxPuts("--- SKIPPED previous DUMP in dump");
        }

        return true;
    }

    bool dumph(wxUint32& pLen, block_header& pHead) // dump magic, length, block header
    {
        if (g_targetDumpCount > m_dumped)
        {
            m_dos->Write32(&g_magic,1);
            m_dos->Write32(&pLen,1);
            m_fos->WriteAll(&pHead.version,80);
            m_fos->WriteAll(m_buffer,pLen-80);
            m_dumped++;
        }
        else
        {
            LOG_DUMP_SKIPPED // wxPuts("--- SKIPPED previous DUMP in dumph");
        }

        return true;
    }

    bool dumpph(wxUint32& pLen, block_header& pHead) // dump magic, length, partial block header
    {
        if (g_targetDumpCount > m_dumped)
        {
            m_dos->Write32(&g_magic,1);
            m_dos->Write32(&pLen,1);
            m_fos->WriteAll(&pHead.version,36);  // only version (4 bytes),  prev block hash (32 bytes)
            m_fos->WriteAll(m_buffer,pLen-36);
            m_dumped++;
        }
        else
        {
            LOG_DUMP_SKIPPED // wxPuts("--- SKIPPED previous DUMP in dumpph");
        }

        return true;
    }

    wxUint32& DumpedCount(){return m_dumped;}

    wxFFileOutputStream& GetFos() { return *m_fos; }
    //wxDataOutputStream& GetDos() { return *m_dos; }

    ~SortedBlockWriter ()
    {
        wxDELETEA(m_buffer);
        wxDELETE (m_dos);
        wxDELETE (m_fos);
    }
};


class SortedHashReader
{
    wxFFile m_file;
    wxFFileInputStream * m_fis;
    wxUint32 m_cnt;
    wxUint32 m_min; // start from hash index
    wxUint32 m_max; // to hash index
    wxString m_hash;
    char hashBuff [65];

public:
    SortedHashReader (wxString pFile) : m_fis(NULL), m_cnt(0), m_min(0), m_max(0)//, m_tgt(0)
    {
        if ( m_file.Open (pFile) )
        {
            m_fis = new wxFFileInputStream(m_file );
        }
        hashBuff [64] = '\0';
    }

    wxString& GetNext ()
    {
        if (m_min > m_max)  // workaround
        {
            g_targetDumpCount++;
        }

        if ( !m_fis->Eof () && m_fis->IsOk () && m_fis->ReadAll( hashBuff, 64 ) )
        {
            m_hash = wxString::From8BitData(hashBuff);
            m_fis ->GetC();
            m_cnt ++;
        }
        else
        {
            m_hash = wxEmptyString;
        }

        return m_hash;
    }

    void SetRange(wxUint32 pMin, wxUint32 pMax) // 4 use with subset of sorted hashes
    {
        m_min = pMin;

        if (pMax == 0)
        {
            m_max = 0;
            g_targetDumpCount = 0;
        }
        else
        {
            m_max = pMax+2;
            g_targetDumpCount = pMax-pMin+1;
        }
    }

    wxUint32& GetMin () {return m_min;}
    wxUint32& GetMax () {return m_max;}
    wxUint32& GetCount () {return m_cnt;}

    ~SortedHashReader ()
    {
        wxDELETE (m_fis);
    }
};


//#define min(a,b)  ((a < b) ? a : b)
//#define max(a,b)  ((a > b) ? a : b)
unsigned char GetNfactor(long long nTimestamp)
{
    int l = 0;

    if (nTimestamp <= 1367991200 /* nChainStartTime*/)
        return 4;

    long long s = nTimestamp - 1367991200 /* nChainStartTime*/;
    while ((s >> 1) > 3) {
      l += 1;
      s >>= 1;
    }

    s &= 3;
    int n = (l * 170 + s * 25 - 2320) / 100;
    if (n < 0) n = 0;
    if (n > 255)    {} // printf( "GetNfactor(%lld) - something wrong(n == %d)\n", nTimestamp, n ); }

    unsigned char N = (unsigned char) n; //printf("GetNfactor: %d -> %d %d : %d / %d\n", nTimestamp - nChainStartTime, l, s, n, min(max(N, minNfactor), maxNfactor));
    //return min(max(N, 4 /*minNfactor*/), 30 /*maxNfactor*/);
    return ((N > 4) ? N : 4);
}


// returns distance of input hash from target height
wxInt32 GetDistance(wxString& pBlockHash, hashMapType& pHashMap, wxUint32 pFromHeight)
{
    hashMapType::iterator it = pHashMap.find( pBlockHash );
    if ( it == pHashMap.end() ) // invalid iterator, block hash not in sorted hashes list, ignore it
    {
        return -1;  //wxPuts( blockHash.ToString() );
    }
    return ( it->second - pFromHeight ); // distance from current height to block
}


// called from UpdateLocMap, iterates over map's elements, decrements block distance from current height and makes a check
wxUint32 CheckLocMap(BlockLocMapType& pMap)
{   //wxPuts("      DECREMENTING MAP");
    wxUint32 mapKey = 0; // return should be greater than 0

    for (BlockLocMapType::iterator it = pMap.begin(); it!= pMap.end(); ++it)    //  map: key=fileOffset, value={block distance from current height, bytesLeft}
    {
        (it->second).dist--;

        if ((it->second).dist == 0) //  we crawled to the height of previously stored block location
        {
            mapKey = it->first;     //  let's return it's file offset
        }
    }
    return mapKey;
}


// Logging helper
class xLogFormatter : public wxLogFormatter
{
    virtual wxString Format (wxLogLevel level, const wxString& msg, const wxLogRecordInfo& info) const
    {
        switch (level)
        {
            case 101:
                return wxString("\n").Append(msg);
            case wxLOG_Info:
                return wxString("INFO: ").Append(msg);//.Append("\n");
            default:
                return msg;

        }
        return msg;
    }
};


// dump blocks from saved stream position, manage map
void UpdateLocMap ( BlockLocMapType& pMap, wxUint32& pHeight, wxFFileInputStream& pFis, SortedBlockWriter& pWriter )
{
    while (!pMap.empty())
    {
        LOG_CHECK_MAP
        wxUint32 locMapKey = CheckLocMap(pMap);

        if ( locMapKey > 0 )
        {
            LOG_DUMP_FROM_MAP

            BlockLocMapType::iterator it = pMap.find(locMapKey);
            pFis.SeekI( it->first, wxFromStart );
            pFis.ReadAll( pWriter.GetBuffer(), (it->second).bytesLeft );
            pWriter.dump( (it->second).bytesLeft );

            pHeight++;
            pMap.erase(locMapKey);
        }
        else
        {
            break;
        }
    }
}

bool CheckFile(wxString& pFileName, wxChar pType)
{

    if ( !wxIsAbsolutePath(pFileName) )
    {
        wxString cwd = wxGetCwd();
        pFileName = cwd.Append("/").Append(pFileName);
    }

    wxFileName fname(pFileName);

    if ( !fname.IsOk() )
    {
        wxLogGeneric(wxLOG_Error,wxString("ERROR: Invalid filename: ").Append(wxEOL + pFileName));
        return false;
    }

    if (pType == 'r')    // readable file
    {
        if ( !fname.FileExists() )
        {
            wxLogGeneric(wxLOG_Error,wxString("ERROR: File not exists: ").Append(wxEOL + pFileName));
            return false;
        }

        if ( !fname.IsFileReadable() )
        {
            wxLogGeneric(wxLOG_Error,wxString("ERROR: File not readable: ").Append(wxEOL + pFileName));
            return false;
        }
    }

    if (pType == 'w')    // writable file
    {
        if ( fname.FileExists() && !fname.IsFileWritable() )
        {
            wxLogGeneric(wxLOG_Error,wxString("ERROR: File not writable: ").Append(wxEOL + pFileName));
        }
    }

    fname.Clear();
    return true;
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//      MAIN
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int main( int argc, char **argv)
{
    LOG_INIT

    wxCmdLineParser parser(argc, argv);
    wxArrayString  cargs;
    wxInt32 lmin, lmax, lskipPercent;

    wxFileOffset skipDatFileBytes = 0;  // fs.SeekI to this position of *.dat file
    wxInt32 scanBlockTreshold = wxINT32_MAX; // quit scaning *.dat file after this many blocks found

    if ( !ProcessCmdLine(parser, cargs, lmin, lmax, lskipPercent, skipDatFileBytes, scanBlockTreshold) )
    {
        wxPuts(parser.GetUsageString());
        return 0;
    }

    if (cargs.size() < 3)
    {
        wxLogGeneric(wxLOG_Error,wxString("ERROR: Too few arguments!"));
        return 0;
    }

    wxString fdat = cargs.Item(0);
    if (!CheckFile(fdat,'r'))
    {
        return 0;
    }

    wxString fsort = cargs.Item(1);
    if (!CheckFile(fsort,'r'))
    {
        return 0;
    }

    wxString fwout = cargs.Item(2);
    if (!CheckFile(fwout,'w'))
    {
        return 0;
    }

    //wxFFileInputStream fs ( "C:\\test.dat" );
    wxFFileInputStream fs ( fdat );
    size_t fsize = fs.GetSize();

    if ( skipDatFileBytes == 0 && lskipPercent > 0 )
    {
        skipDatFileBytes = fsize/100*lskipPercent;
    }
    else if ( skipDatFileBytes > fsize )
    {
        wxLogGeneric(wxLOG_Error,wxString("ERROR: skip bytes value must be less than file size: ").Append(wxString::Format("%i",fsize)));

        if (fs.GetFile()->IsOpened())
        {
            fs.GetFile()->Close();
        }

        return 0;
    }

    //SortedHashReader sortedHashes("C:\\y700k1");
    SortedHashReader sortedHashes(fsort);
    sortedHashes.SetRange(lmin,lmax);

    //SortedBlockWriter fout ("C:\\sorted.dat");
    SortedBlockWriter fout (fwout);

    wxPuts( fdat );
    wxPuts( fsort );
    wxPuts( fwout );

    wxPuts(wxEmptyString);
    wxPuts(wxString("Sorted block hash range:   ").Append(wxString::Format("%i",lmin)).Append(" .. ").Append(wxString::Format("%i",lmax)));
    wxPuts(wxString("start read from byte:      ").Append(wxString::Format("%i",skipDatFileBytes)).Append("  [of ").Append(wxString::Format("%i",fs.GetSize()).Append("]")));
    wxPuts(wxString("max blocks scanned:        ").Append(wxString::Format("%i",scanBlockTreshold)));
    wxPuts(wxEmptyString);


    //SortedHashReader sortedHashes("C:\\y20");
    //SortedHashReader sortedHashes("C:\\y10k1");

    // use 0 .. n when block 0 is needed in written dump file
    // use 1 .. n when extracting from 10000 ..

    //sortedHashes.SetRange(1,1);    // dump block 0 only - need "0000000000..." not "0000060fc9...." in first line of sorted hash file!!!
    //sortedHashes.SetRange(0,0);    // dump block 0 only - need "0000060fc9..." not "0000000000...." in first line of sorted hash file!!!

    //sortedHashes.SetRange(0,10);    // dump blocks 0 to 10 (=11 blocks)

    //sortedHashes.SetRange(1,5);    // dump 5 blocks from parents of 1st sorted hash to the end; need "00000000..." in first line for block 0000060fc9...
    //sortedHashes.SetRange(0,4);    // dump 5 blocks from parents of 1st sorted hash to the end; need "00000000..." in first line for block 0000060fc9...

    //sortedHashes.SetRange(1,0);    // dump all blocks; need "00000000..." in first line for block 0000060fc9...
    //sortedHashes.SetRange(3,0);  // dump all blocks from 3rd on (skip first 2)

    //sortedHashes.SetRange(1,0);


    // for i in {0..10}; do yacoind getblockhash $i; done
    // sortedHashes.SetRange(0,10);
    // for i in {10..20}; do yacoind getblockhash $i; done
    // sortedHashes.SetRange(1,10);
    // sortedHashes.SetRange(11,20); // long file with sorted hashes - need "0000060fc9..." not "0000000000...." in first line of sorted hash file!!!

    //wxPuts(wxString::Format("%i",skipToFileOffset));
    //wxFFileInputStream fs ( "C:\\out3x7x3.dat" );

    hashMapType blockHashMap;
    hashMapType multiPrevHashMap; // multiple prev hash occurencies

    BlockLocMapType locMap;
    BlockFileLocType floc;

    block_header bh;

    wxDataInputStream ds (fs);
    wxFileOffset foff;

    bool skipSeek;
    wxInt32 dist;   // block distance from current height
    wxInt32 rBlksCnt = 0;  // read blocks count
    wxUint32 wBlksCnt = 0;  // written blocks count
    unsigned char hashBuff[32];
    char hash64[64];

    wxUint32 uiStreamBuff;
    char * buffer = reinterpret_cast <char *>(&uiStreamBuff);


    ////////////////////////////////////////////////////////////////////////////////////////////////////
    // PART 1
    // scan input *.dat file denoting multiple blocks with same hashPrevBlock references
    // temporary store hashPrevBlock in blockHashMap
    // store siblings (references of multiple blocks with same hashPrevBlock value) in multiPrevHashMap
    ////////////////////////////////////////////////////////////////////////////////////////////////////

    fs.SeekI(skipDatFileBytes);
    uiStreamBuff = ds.Read32();     // read first 4 bytes from input stream

    while ( !fs.Eof() )
    {
        if ( uiStreamBuff == g_magic ) // magic found
        {
            rBlksCnt++;
            ds.Read32( &floc.bytesLeft, 1 );        // Blocksize number of bytes following up to end of block (4 bytes)
            fs.SeekI( 4, wxFromCurrent );           // skip Version   Block version number (4 bytes)
            fs.ReadAll( &bh.prev_block, 32 );       // hashPrevBlock 256-bit hash of the previous block header (32 bytes)
            fs.SeekI( floc.bytesLeft-36, wxFromCurrent ); // FFWD to the end of block

            //  uint256 prevBlockHash = uint256( std::vector<unsigned char>(bh.prev_block, bh.prev_block+32) );
            //  wxString prevBlockHashString ( prevBlockHash.ToString() ); // std::string -> wxString

            for (wxUint32 i=0; i<32; i++)   // reverse bytes
            {
                sprintf( hash64 + i*2, "%02x", bh.prev_block[31-i] );
            }
            wxString prevBlockHashString  = wxString::From8BitData( hash64, 64) ;

            wxUint32 hcnt = blockHashMap.count(prevBlockHashString);
            blockHashMap[prevBlockHashString] = hcnt + 1;

            LOG_PREV_BLOCK_HASH

            uiStreamBuff = ds.Read32();  // read next 4 bytes from fstream - magic?
        }
        else
        {   // move further on stream position, looking for magic
            uiStreamBuff >>= 8; // works on Little Endian
            buffer[3] = ds.Read8();
        }
        if ( rBlksCnt > scanBlockTreshold )
        {
            break ;
        }
    }

    wxLogGeneric(wxLOG_Info,wxString("prev_block distinct count: ").Append(wxString::Format( "%i",blockHashMap.size())));

    //  put siblings in new hash map
    for (hashMapType::iterator it = blockHashMap.begin(); it!= blockHashMap.end(); ++it)
    {
        if (it->second > 1) // prev hash occurencies
        {
            multiPrevHashMap[it->first] = 0;
            LOG_PREV_BLOCK_MULTIPLE
        }
    }

    wxLogGeneric(wxLOG_Info,wxString("prev_block duplicate count: ").Append(wxString::Format( "%i",multiPrevHashMap.size()))); // hash duplicates


    ///////////////////////////////////////////////////////////////////////////////////////////
    // PART 2
    // Load sorted hash values into memory
    // limit their range with input (command line) parameters -> sortedHashes.SetRange(min,max)
    ///////////////////////////////////////////////////////////////////////////////////////////
    blockHashMap.clear();   // let's reuse this hash map
    wxUint32 j = 1;
    wxUint32 sortedHashHeight = 0;
    do
    {
        wxString n = sortedHashes.GetNext();

        if (n.IsEmpty())
        {
            break;
        }
        else
        {
            if ( j >= sortedHashes.GetMin() && ( sortedHashes.GetMax() == 0 || j < sortedHashes.GetMax() ) )
            {
                blockHashMap[n] = sortedHashHeight++;
                LOG_SORTED_HASH_READ
            }
            j++;
        }
    } while (true);

    wxLogGeneric(wxLOG_Info,wxString("sorted hash read count: ").Append(wxString::Format("%i",blockHashMap.size())));


    ////////////////////////////////////////////////////////////////////////////////////////////////
    // PART 3
    // Second pass over input *.dat file this time comparing to sorted hashes and writing them down
    // Blocks with header hashes previously detected in multiple succeeding blocks (branches)
    // get hashed to determine if hash is listed in sorted list
    ////////////////////////////////////////////////////////////////////////////////////////////////
    rBlksCnt = 0;
    fs.Reset();
    fs.SeekI(skipDatFileBytes);
    uiStreamBuff = ds.Read32();     // read first 4 bytes from input stream

    while ( !fs.Eof() )
    {
        if ( uiStreamBuff == g_magic )
        {
            skipSeek = false;
            rBlksCnt++;
            ds.Read32( &floc.bytesLeft, 1 );      // Blocksize   number of bytes following up to end of block (4 bytes)
            foff = fs.TellI();
            ds.Read32( &bh.version, 1 );           // Version   Block version number (4 bytes)
            fs.ReadAll( &bh.prev_block, 32 );      // hashPrevBlock 256-bit hash of the previous block header (32 bytes)

            //uint256 prevBlockHash = uint256( std::vector<unsigned char>(bh.prev_block, bh.prev_block+32) );
            //wxString prevBlockHashString ( prevBlockHash.ToString() ); // std::string -> wxString

            for (wxUint32 i=0; i<32; i++)   // reverse bytes
            {
                sprintf( hash64 + i*2, "%02x", bh.prev_block[31-i] );
            }
            wxString prevBlockHashString  = wxString::From8BitData(hash64,64);

            if ( blockHashMap.count(prevBlockHashString) == 0 )    // parent hash not found in sorted hashes list
            {
                if ( sortedHashes.GetMin() == 0 && prevBlockHashString.IsSameAs("0000000000000000000000000000000000000000000000000000000000000000") )
                {
                    if ( blockHashMap.count("0000060fc90618113cde415ead019a1052a9abc43afcccff38608ff8751353e5") == 1 )  // yacoind getblockhash 0
                    {
                        LOG_DUMP_FIRST_BLOCK
                        skipSeek = fout.dump0();   // for now leave stream position here
                        //wBlksCnt++;   // do not ADVANCE block 0
                    }

                }
                else    // forget this block with nonexistent parent
                {
                    LOG_HASH_WITHOUT_PARENT
                }

                if (!skipSeek)
                {
                    fs.SeekI( floc.bytesLeft-36, wxFromCurrent ); // fast FWD to the end of block
                }
            }
            else    // prev_hash found in sorted block list
            {   LOG_PREV_HASH_IN_SORTLIST
                wxUint32 hcnt = multiPrevHashMap.count( prevBlockHashString );  // check if it has siblings

                if ( hcnt > 0 ) // looks like branch block - many share the same parent, need to hash it to find out if it is valid
                {
                    fs.ReadAll( &bh.merkle_root, 32 );  // hashMerkleRoot 256-bit (32 bytes)
                    ds.Read32( &bh.timestamp, 1 );      // Time       Current timestamp as seconds since 1970-01-01T00:00 UTC (4 bytes)
                    ds.Read32( &bh.bits, 1 );           // (4 bytes)
                    ds.Read32( &bh.nonce, 1 );          // (4 bytes)

                    //uint256 blockHash;
                    //scrypt_hash((void*)&bh.version, sizeof(bh), (unsigned int *)&blockHash, GetNfactor(bh.timestamp)); // go Jane go go go
                    scrypt_hash((void*)&bh.version, sizeof(bh), (unsigned int *)hashBuff, GetNfactor(bh.timestamp)); // go Jane go go go

                    // reverse bytes
                    for (wxUint32 i=0; i<32; i++)
                    {
                        sprintf( hash64 + i*2, "%02x", hashBuff[31-i] );
                    }

                    //dist = GetDistance( wxString( blockHash.ToString() ), blockHashMap, wBlksCnt+1 );  // if hash not exists in sorted hash list: dist = -1
                    wxString s64 = wxString::From8BitData( hash64,64 );
                    dist = GetDistance( s64, blockHashMap, wBlksCnt+1 );  // if hash not exists in sorted hash list: dist = -1

                    LOG_HASH_JANE

                    if (dist == 0) // correct sibling found at current height
                    {
                        LOG_DUMP_JANE

                        fs.ReadAll( fout.GetBuffer(), floc.bytesLeft-80 );
                        skipSeek = fout.dumph( floc.bytesLeft, bh );
                        wBlksCnt++;

                        foff = fs.TellI();
                        UpdateLocMap( locMap, wBlksCnt, fs, fout);
                        if ( fs.TellI() != foff )
                        {
                            fs.SeekI(foff, wxFromStart);
                        }

                    }
                    else if ( dist > 0 )    // correct sibling found at distant height, save it's file location for later
                    {
                        floc.dist = dist;
                        locMap[foff] = floc;

                        LOG_SIBLING_TO_MAP
                    }
                    else    // -1, no hash found in sorted list, maybe it's a ball not a block?
                    {
                            LOG_HASH_NOT_IN_MAP
                    }

                    if (!skipSeek)
                    {
                            fs.SeekI( floc.bytesLeft-80, wxFromCurrent ); // fast FWD to the end of block
                    }

                }
                else // block without siblings, let's find it's position
                {
                    dist = GetDistance( prevBlockHashString, blockHashMap, wBlksCnt );

                    LOG_HASH_ONLY_SON

                    if (dist == 0)  // correct block found at current height
                    {
                        LOG_DUMP_ONLY_SON
                        fs.ReadAll( fout.GetBuffer(), floc.bytesLeft-36 );
                        skipSeek = fout.dumpph( floc.bytesLeft, bh );
                        wBlksCnt++;

                        foff = fs.TellI();
                        UpdateLocMap( locMap, wBlksCnt, fs, fout);
                        if ( fs.TellI() != foff )
                        {
                            fs.SeekI(foff, wxFromStart);
                        }

                    }
                    else if ( dist > 0 )    // correct block found at distant height, save it's file location for later
                    {
                        floc.dist = dist;
                        locMap[foff] = floc;

                        LOG_ONLY_SON_TO_MAP
                    }

                    if (!skipSeek)
                    {
                            fs.SeekI( floc.bytesLeft-36, wxFromCurrent );   // fast FWD to the end of block
                    }
                }
            }

            uiStreamBuff = ds.Read32();  // read next 4 bytes from fstream, we should get magic number?
        }
        else // get next byte from file stream
        {   //wxPuts("this really shouldn't happen often");
            uiStreamBuff >>= 8; // Little Endian
            buffer[3] = ds.Read8();
        }

        //wxPuts(wxString("---------- g_targetDumpCount: ").Append(wxString::Format("%i",g_targetDumpCount)));
        //wxPuts(wxString("---------- fout.Dumped(): ").Append(wxString::Format("%i",fout.Dumped())));

        if ( rBlksCnt > scanBlockTreshold || fout.DumpedCount() > g_targetDumpCount )
        {
            break;
        }
    }

    wxLogGeneric(wxLOG_Info,wxString("sorted hash write count: ").Append(wxString::Format("%i",fout.DumpedCount())));
    return 0;
}

