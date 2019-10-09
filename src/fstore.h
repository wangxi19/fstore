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
    explicit FStoreMetaData() {

    }

    ~FStoreMetaData() {

    }

    uint32_t createTm{0};
    uint32_t changeTm{0};
    uint64_t allocSz{0};
    uint64_t useSz{0};//alias: write sizes
    uint8_t userData[1000]{0, };
}__attribute__((packed));

class FStore {
public:
    explicit FStore();
    ~FStore();

    bool Init(const std::string& iFStoreRootPath,
              uint64_t iPerFileSzBytes,
              uint32_t iFileNumbers);

    //to do,
    //May can add a cache buffer, write data to buffer, and write buffer data to store file when buffer data is filled

    /*
     * if iCrossFile is true, iData can be stored in multiple files if it is necessary
    */

    //tips: when a store file will be filled, the store file's and the next store file's fds will be returned
    bool StoreData(const char* iData, uint64_t iDatalen, bool iCrossFile = false);

    bool PartitionFiles();

    void WriteUserData(const uint8_t* iUserData, uint8_t iSz);

private:
    std::string mFStoreRootPath;

    //file system block size alignment
    uint64_t mPerFileSzBytes{0};
    uint32_t mFileNumbers{0};

    uint32_t mFSBlockSz{0};

    std::vector<std::string> mFileEntryLst;
    std::string mCurFileName;
    int mCurFileFd{0};
    uint64_t mCurFileFillSize{0};
};

#endif //FSTORE_H
