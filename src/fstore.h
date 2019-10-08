#ifndef FSTORE_H
#define FSTORE_H

#include <string>
#include <time.h>

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

    bool PartitionFiles();

    void WriteUserData(const uint8_t* iUserData, uint8_t iSz);

private:
    std::string mFStoreRootPath;

    //file system block size alignment
    uint64_t mPerFileSzBytes{0};
    uint32_t mFileNumbers{0};

    uint32_t mFSBlockSz{0};
};

#endif //FSTORE_H
