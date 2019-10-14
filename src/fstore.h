#ifndef FSTORE_H
#define FSTORE_H

#include <string>
#include <time.h>
#include <vector>

//#define LOGERROR(fmt, args...) \
//       fprintf(stderr, "[ERROR] %l %s:%s:%d " fmt, time(NULL), __FILE__, __FUNCTION__, __LINE__, ##args);

#define LOGERROR(fmt, args...) \
    fprintf(stderr,"[ERROR] %s %s:%d " fmt "\n", __FILE__, __FUNCTION__, __LINE__, ##args);

#define LOGINFO(fmt, args...) \
    fprintf(stderr,"[INFO] %s %s:%d " fmt "\n", __FILE__, __FUNCTION__, __LINE__, ##args);

struct FStoreMetaData
{
    enum STATUSENUM {
        STATUS_FREE = 0,
        STATUS_USING = 1,
        STATUS_DONE = 2
    };

    explicit FStoreMetaData() {

    }

    ~FStoreMetaData() {

    }

    uint32_t fileno{0};
    uint32_t createTm{0};
    uint64_t allocSz{0};

    uint32_t changeTm{0};
    uint64_t useSz{0};//alias: writing sizes
    uint8_t status{0};
#define FStoreMetaDataUserDataLen 1000
    uint8_t userData[FStoreMetaDataUserDataLen]{0, };
}__attribute__((packed));

#define PFNWriteRelDataCallBackFLAG_NORMAL 0
#define PFNWriteRelDataCallBackFLAG_BEFOR_FIRST 1
#define PFNWriteRelDataCallBackFLAG_AFTER_FIRST 2
#define PFNWriteRelDataCallBackFLAG_BEFOR_LAST 3
#define PFNWriteRelDataCallBackFLAG_AFTER_LAST 4

typedef bool (*PFNWriteRelDataCallBack) (const uint8_t* iData,
                                         uint64_t iDatalen,
                                         bool iCrossFiles,
                                         uint8_t iFlag,
                                         void* iFStorePtr,
                                         void* iContext);

/*
*                   Tips: brain storm
*   for efficency, using double indices: a standalone index file and individual index in each store file
*   meta data was written into index file firstly,
*   and write it from index file to each store file as soon as store file status became STATUS_DONE.
*   the advantages is: writing file offset will only be increased, it is good for protecting disk and improving writing speed
*
*   but the disadvantages is: the meta data will be written into each store file only the store file has finished writing.
*   the updating of meta data isn't in real time.
*/

class FStore {
public:
    explicit FStore();
    ~FStore();

    bool Init(const std::string& iFStoreRootPath,
              uint64_t iPerFileSzBytes,
              uint32_t iFileNumbers);


    /*
     * if iCrossFile is true, iData can be stored in multiple files if it is necessary
    */
    bool IsFirstWriting(uint64_t iLenToBeWritten, bool iCrossFiles);
    /*
    *
    * if iCrossFiles is false:
    *       return true only iLenToBeWritten is equal to the free space of store file
    *       return false if iLenToBeWritten less than or greater than the free space of store file
    *
    * if iCrossFiles is true:
    *       return true if iLenToBeWritten is equal to or greater than the free space of store file
    *       return false only iLenToBeWritten is less than the free space of store file
    */
    bool IsLastWriting(uint64_t iLenToBeWritten, bool iCrossFiles);

    //to do,
    //May can add a cache buffer, write data to buffer, and write buffer data to store file when buffer data is filled

    /*
     * if iCrossFile is true, iData can be stored in multiple files if it is necessary
     *
     * if iPfn is nullptr, then mPfnWtRDCBK is used
     *
     * Note: iDatalen must be less than mPerFileRelSpaceSzBytes
    */
    bool WriteRelData(const uint8_t* iData, uint64_t iDatalen, bool iCrossFiles = false,
                      PFNWriteRelDataCallBack iPfn = nullptr,
                      void* iPfnContext = nullptr);

    bool PartitionFiles();

    //the iOfstFromMetaDta must less than sizeof(FStoreMetaData::userData)
    bool WriteUserData(const uint8_t* iUserData, uint8_t iSz, uint32_t iOfstFromUsrDta);//write in current store file
    bool WriteLastStorFileUserData(const uint8_t* iUserData, uint8_t iSz, uint32_t iOfstFromUsrDta);//write in last store file
    //can use WriteMetaData to write userdata, because userdata is following with metadata
    //the iOfstFromMetaDta must less than sizeof(FStoreMetaData)
    bool WriteMetaData(const uint8_t* iMetaData, uint8_t iSz, uint32_t iOfstFromMetaDta);//write in current store file
    bool WriteLastStorFileMetaData(const uint8_t* iMetaData, uint8_t iSz, uint32_t iOfstFromMetaDta);//write in last store file

    const std::vector<FStoreMetaData>& fStorIndices() const;

    //indices is matching to each of store file's index
    bool ExistValidFS();
private:
    bool __writeMetaData(const uint8_t* iMetaData, uint8_t iSz, uint32_t iOfstFromMetaDta, int fd);
    bool __writeUserData(const uint8_t* iUserData, uint8_t iSz, uint32_t iOfstFromUsrDta, int fd);
    bool useNextStoreFile();

    /*
    *
    *  [0, mFileQuantities)
    *  0 <= iFrom <= iTo < mFileQuantities
    */
    bool syncMemToIndices(int iFrom, int iTo);
    bool syncIndicesToMem();

    bool openIndices();
    bool closeIndices();
private:
    std::string mFStoreRootPath;

    //mPerFileSzBytes is aligned with FS(file system) block size(mFSBlockSz)
    uint64_t mPerFileSzBytes{0};
    uint32_t mFileQuantities{0};
    uint64_t mStorFileRelSpaceSzBytes{0};

    uint32_t mFSBlockSz{0};

    std::vector<std::string> mStorFileEntryLst;
    int mCurFileNo{-1};
    int mCurStorFileFd{-1};
    int mLastStorFileFd{-1};
    uint64_t mCurFileFillSize{0};
    uint64_t mLastFileFillSize{0};

    PFNWriteRelDataCallBack mPfnWtRDCBK{nullptr};
    std::vector<FStoreMetaData> mFStorIndices;
    int mIndicesFd{-1};
};

#endif //FSTORE_H
