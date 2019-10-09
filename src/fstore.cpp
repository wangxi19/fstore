#include "fstore.h"
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

FStore::FStore()
{

}

FStore::~FStore()
{

}

bool FStore::Init(const std::string &iFStoreRootPath, uint64_t iPerFileSzBytes, uint32_t iFileNumbers)
{
    mFStoreRootPath = iFStoreRootPath;

    if (0 != access(mFStoreRootPath.c_str(), F_OK)) {
        LOGERROR("Folder %s does not exists\n", mFStoreRootPath.c_str());
        return false;
    }

    if (0 != access(mFStoreRootPath.c_str(), W_OK)) {
        LOGERROR("Fail to grant write permission from folder %s\n", mFStoreRootPath.c_str());
        return false;
    }

    if (0 != access(mFStoreRootPath.c_str(), R_OK)) {
        LOGERROR("Fail to grant read permission from folder %s\n", mFStoreRootPath.c_str());
        return false;
    }

    struct stat statBuf;
    memset(&statBuf, 0, sizeof(struct stat));
    if (0 != stat(mFStoreRootPath.c_str(), &statBuf)) {
        LOGERROR("Fail to execute stat on folder %s\n", mFStoreRootPath.c_str());
        return false;
    }

    if (!S_ISDIR(statBuf.st_mode)) {
        LOGERROR("%s is not a folder\n", mFStoreRootPath.c_str());
        return false;
    }



    mPerFileSzBytes = iPerFileSzBytes;
    mFileNumbers = iFileNumbers;
    mFSBlockSz = (uint32_t) statBuf.st_blksize;

    auto remainder = mPerFileSzBytes % mFSBlockSz;

    //align to block size
    if (remainder != 0) {
        mPerFileSzBytes = mPerFileSzBytes - remainder + mFSBlockSz;
    }

    return true;
}

bool FStore::StoreData(const char *iData, uint64_t iDatalen, bool iCrossFile)
{
    //FStoreMetaData will be appended at each store file's tail
    if (mCurFileFillSize + iDatalen > (mPerFileSzBytes - sizeof(FStoreMetaData))) {

    }
}

bool FStore::PartitionFiles()
{
    std::string tmFileName;
    FILE* fd{nullptr};
    int errNo{0};
    for (uint32_t i = 0; i < mFileNumbers; i++) {
        tmFileName = mFStoreRootPath + "/" + std::to_string(i) + ".fstor";
        fd = fopen(tmFileName.c_str(), "w+");
        if (fd == nullptr) {
            LOGERROR("Fail to partition file; on folder: %s; at file no: %d\n",mFStoreRootPath.c_str(), i);
            return false;
        }

        if ((errNo = posix_fallocate64(fileno(fd), 0, mPerFileSzBytes)) != 0) {
            LOGERROR("Fail to allocate file: %s; errno: %d\n", tmFileName.c_str(), errNo);
            return false;
        }

        if (0 != fclose(fd)) {
            LOGERROR("Fail to close file descriptor: %s\n", tmFileName.c_str());
            return false;
        }

        mFileEntryLst.push_back(tmFileName);
    }

    return true;
}

void FStore::WriteUserData(const uint8_t *iUserData, uint8_t iSz)
{
    (void)iUserData;
    (void)iSz;
}
