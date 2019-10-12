#ifndef FSTORE_H
#define FSTORE_H

#include <string>
#include <time.h>
#include <vector>

//#define LOGERROR(fmt, args...) \
//       fprintf(stderr, "[ERROR] %l %s:%s:%d " fmt, time(NULL), __FILE__, __FUNCTION__, __LINE__, ##args);

#define LOGERROR(fmt, args...) \
    fprintf(stderr,"[ERROR] %s %s:%d " fmt "\n", __FILE__, __FUNCTION__, __LINE__, ##args);

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
    uint8_t userData[1000]{0, };
}__attribute__((packed));


#define PFNWriteRelDataCallBackFLAG_BEFOR_FIRST 1
#define PFNWriteRelDataCallBackFLAG_AFTER_FIRST 2
#define PFNWriteRelDataCallBackFLAG_BEFOR_LAST 3
#define PFNWriteRelDataCallBackFLAG_AFTER_LAST 4

typedef bool (*PFNWriteRelDataCallBack) (const uint8_t* iData,
                                         uint64_t iDatalen,
                                         bool iCrossFiles,
                                         uint8_t iFlag,
                                         void* iFStorePtr,
                                         void* iUserData);

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
    */
    bool WriteRelData(const uint8_t* iData, uint64_t iDatalen, bool iCrossFiles = false, PFNWriteRelDataCallBack iPfn = nullptr);

    bool PartitionFiles();

    //the iOfstFromMetaDta must less than sizeof(FStoreMetaData::userData)
    void WriteUserData(const uint8_t* iUserData, uint8_t iSz, uint32_t iOfstFromUsrDta);//write in current store file
    void WriteLastStorFileUserData(const uint8_t* iUserData, uint8_t iSz, uint32_t iOfstFromUsrDta);//write in last store file
    //can use WriteMetaData to write userdata, because userdata is following with metadata
    //the iOfstFromMetaDta must less than sizeof(FStoreMetaData)
    void WriteMetaData(const uint8_t* iMetaData, uint8_t iSz, uint32_t iOfstFromMetaDta);//write in current store file
    void WriteLastStorFileMetaData(const uint8_t* iMetaData, uint8_t iSz, uint32_t iOfstFromMetaDta);//write in last store file

private:
    std::string mFStoreRootPath;

    //mPerFileSzBytes is aligned with FS(file system) block size(mFSBlockSz)
    uint64_t mPerFileSzBytes{0};
    uint32_t mFileQuantities{0};

    uint32_t mFSBlockSz{0};

    std::vector<std::string> mFileEntryLst;
    std::string mCurFileName;
    int mCurStorFileFd{0};
    int mLastStorFileFd{0};
    uint64_t mCurFileFillSize{0};

    PFNWriteRelDataCallBack mPfnWtRDCBK{nullptr};
};

#endif //FSTORE_H
