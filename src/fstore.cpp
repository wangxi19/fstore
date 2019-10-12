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
    mFileQuantities = iFileNumbers;
    mFSBlockSz = (uint32_t) statBuf.st_blksize;

    auto remainder = mPerFileSzBytes % mFSBlockSz;

    //align to block size
    if (remainder != 0) {
        mPerFileSzBytes = mPerFileSzBytes - remainder + mFSBlockSz;
    }

    return true;
}

bool FStore::IsFirstWriting(uint64_t iLenToBeWritten, bool iCrossFiles)
{

}

bool FStore::IsLastWriting(uint64_t iLenToBeWritten, bool iCrossFiles)
{

}

bool FStore::WriteRelData(const uint8_t *iData, uint64_t iDatalen, bool iCrossFiles, PFNWriteRelDataCallBack iPfn)
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
    for (uint32_t i = 0; i < mFileQuantities; i++) {
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

bool FStore::WriteUserData(const uint8_t *iUserData, uint8_t iSz, uint32_t iOfstFromUsrDta)
{
    (void)iUserData;
    (void)iSz;
}

bool FStore::WriteLastStorFileUserData(const uint8_t *iUserData, uint8_t iSz, uint32_t iOfstFromUsrDta)
{

}

bool FStore::WriteMetaData(const uint8_t *iMetaData, uint8_t iSz, uint32_t iOfstFromMetaDta)
{
    if (!__writeMetaData(iMetaData, iSz, iOfstFromMetaDta, mCurStorFileFd)) {
        LOGERROR("Fail to write metadata \n");
        return false;
    }
    return true;
}

bool FStore::WriteLastStorFileMetaData(const uint8_t *iMetaData, uint8_t iSz, uint32_t iOfstFromMetaDta)
{

}

const std::vector<FStoreMetaData> &FStore::fStorIndices() const
{
    return mFStorIndices;
}

bool FStore::__writeMetaData(const uint8_t *iMetaData, uint8_t iSz, uint32_t iOfstFromMetaDta, int fd)
{
    bool ret{false};
    if (iSz == 0) {
        ret = true;
        goto end;
    }

    if (fd <= 0) {
        LOGERROR("fd %d <= 0; fd is less than or equal to 0\n", fd)
        goto end;
    }

    if (iSz + iOfstFromMetaDta > sizeof(FStoreMetaData)) {
        LOGERROR("iSz + iOfstFromMetaDta %u >= sizeof(FStoreMetaData)\n", iOfstFromMetaDta)
        goto end;
    }

    if (-1 == lseek(fd, - (int)sizeof(FStoreMetaData) - 1 +  (int)iOfstFromMetaDta, SEEK_END)) {
        LOGERROR("Fail to lseek %d SEEK_END, errno %d\n", (- (int)sizeof(FStoreMetaData) - 1 +  (int)iOfstFromMetaDta), errno);
        goto end;
    }

    if (iSz != write(fd, iMetaData, iSz)) {
        LOGERROR("Fail to write %u bytes meta data to current store file, errno %d\n", iSz, errno);
        goto end;
    }

    ret = true;

    end:

    return ret;
}
