#ifndef FSTORE_H
#define FSTORE_H

#include <string>
#include <time.h>
#include <vector>

#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>

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
    uint64_t useSz{0};//alias: written sizes
    uint8_t status{0};
#define FStoreMetaDataUserDataLen 995
    uint8_t userData[FStoreMetaDataUserDataLen]{0, };
}__attribute__((packed));

#define PFNWriteRelDataCallBackFLAG_NORMAL 0
#define PFNWriteRelDataCallBackFLAG_BEFOR_FIRST 1 //after opening new file, before writing data to file.
#define PFNWriteRelDataCallBackFLAG_AFTER_FIRST 2 //after opening new file, after writing data to file. other words, the firstly writing data to file
#define PFNWriteRelDataCallBackFLAG_BEFOR_LAST 3  //before opening new file. Only validation when current file is valid
#define PFNWriteRelDataCallBackFLAG_AFTER_LAST 4  //after opening new file, before writing data to file. Only validation when current file is valid

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
*   the advantages are: writing file offset will only be increased, it is good for protecting disk and improving writing speed
*
*   but the disadvantages are: the meta data will be written into each store file only the store file has finished writing.
*   the updating of meta data isn't in real time.
*/

class FStore {
public:
    explicit FStore()
    {

    }
    ~FStore()
    {
        if (mIndicesMap != 0) {
            munmap(mIndicesMap, mIndicesMapSz);
            mIndicesMap = 0;
        }
    }

    bool Init(const std::string& iFStoreRootPath,
              uint64_t iPerFileSzBytes,
              uint32_t iFileNumbers)
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

        mStorFileRelSpaceSzBytes = mPerFileSzBytes - sizeof(FStoreMetaData);

        return true;
    }


    /*
     * if iCrossFile is true, iData can be stored in multiple files if it is necessary
    */
    bool IsFirstWriting(uint64_t iLenToBeWritten, bool iCrossFiles)
    {
        //    if (!iCrossFiles) {
        //        if (iLenToBeWritten + mCurFileFillSize > mStorFileRelSpaceSzBytes) {
        //            return true;
        //        }

        //        return false;
        //    } else {
        //        if (iLenToBeWritten + mCurFileFillSize > mStorFileRelSpaceSzBytes) {
        //            return true;
        //        }

        //        return false;
        //    }
        (void)(iCrossFiles);
        if (iLenToBeWritten + mCurFileFillSize > mStorFileRelSpaceSzBytes) {
            return true;
        }

        return false;
    }
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
    bool IsLastWriting(uint64_t iLenToBeWritten, bool iCrossFiles)
    {
        if (!iCrossFiles) {
            if (iLenToBeWritten + mCurFileFillSize == mStorFileRelSpaceSzBytes) {
                return true;
            }
        } else {
            if (iLenToBeWritten + mCurFileFillSize >= mStorFileRelSpaceSzBytes) {
                return true;
            }
        }

        return false;
    }

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
                      void* iPfnContext = nullptr)
    {
        bool usedNexStorFle {false};
        if (mCurStorFileFd < 0) {
            if (!useNextStoreFile()) {
                LOGERROR("Fail to call WriteRelData\n");
                return false;
            }

            usedNexStorFle = true;
            if (iPfn)
                iPfn(iData, iDatalen, iCrossFiles, PFNWriteRelDataCallBackFLAG_BEFOR_FIRST, this, iPfnContext);
        }

        if (!iCrossFiles) {
            if (iDatalen + mCurFileFillSize > mStorFileRelSpaceSzBytes) {

                if (iPfn)
                    iPfn(iData, iDatalen, iCrossFiles, PFNWriteRelDataCallBackFLAG_BEFOR_LAST, this, iPfnContext);

                if (!useNextStoreFile()) {
                    LOGERROR("Fail to call WriteRelData\n");
                    return false;
                }

                if (iPfn)
                    iPfn(iData, iDatalen, iCrossFiles, PFNWriteRelDataCallBackFLAG_AFTER_LAST, this, iPfnContext);

                usedNexStorFle = true;
                if (iPfn)
                    iPfn(iData, iDatalen, iCrossFiles, PFNWriteRelDataCallBackFLAG_BEFOR_FIRST, this, iPfnContext);
            }

            if (iPfn)
                iPfn(iData, iDatalen, iCrossFiles, PFNWriteRelDataCallBackFLAG_NORMAL, this, iPfnContext);

            auto cpos = lseek(mCurStorFileFd, 0, SEEK_CUR);
            if (cpos != mCurFileFillSize) {
                LOGERROR("Some error occured. current fd position isn't equal to mCurFileFillSize\n");
            }

            if (iDatalen != (uint64_t)write(mCurStorFileFd, iData, iDatalen)) {
                lseek(mCurStorFileFd, cpos, SEEK_SET);
                LOGERROR("Fail to write %lu real data to current store file\n", iDatalen);
                return false;
            }

            mCurFileFillSize += iDatalen;
            if (usedNexStorFle) {
                if (iPfn) iPfn(iData, iDatalen, iCrossFiles, PFNWriteRelDataCallBackFLAG_AFTER_FIRST, this, iPfnContext);
            }
        } else {
            auto wtsz = mStorFileRelSpaceSzBytes - mCurFileFillSize;
            wtsz = wtsz > iDatalen ? iDatalen : wtsz;
            auto remainderSz = iDatalen > wtsz ? (iDatalen - wtsz) : 0;

            if (iPfn) iPfn(iData, wtsz, iCrossFiles, PFNWriteRelDataCallBackFLAG_NORMAL, this, iPfnContext);

            auto cpos = lseek(mCurStorFileFd, 0, SEEK_CUR);
            if (cpos != mCurFileFillSize) {
                LOGERROR("Some error occured. current fd position isn't equal to mCurFileFillSize\n");
            }

            if (wtsz != (uint64_t)write(mCurStorFileFd, iData, wtsz)) {
                lseek(mCurStorFileFd, cpos, SEEK_SET);
                LOGERROR("Fail to write %lu real data to current store file, totoal data length %lu\n", wtsz, iDatalen);
                return false;
            }

            if (usedNexStorFle) {
                if (iPfn) iPfn(iData, wtsz, iCrossFiles, PFNWriteRelDataCallBackFLAG_AFTER_FIRST, this, iPfnContext);
            }

            mCurFileFillSize += wtsz;
            if (remainderSz > 0) {

                if (iPfn) iPfn(iData + wtsz, remainderSz, iCrossFiles, PFNWriteRelDataCallBackFLAG_BEFOR_LAST, this, iPfnContext);
                if (!useNextStoreFile()) {
                    LOGERROR("Fail to call WriteRelData\n");
                    return false;
                }
                if (iPfn) iPfn(iData + wtsz, remainderSz, iCrossFiles, PFNWriteRelDataCallBackFLAG_AFTER_LAST, this, iPfnContext);

                usedNexStorFle = true;
                if (iPfn) iPfn(iData + wtsz, remainderSz, iCrossFiles, PFNWriteRelDataCallBackFLAG_BEFOR_FIRST, this, iPfnContext);

                if (iPfn) iPfn(iData + wtsz, remainderSz, iCrossFiles, PFNWriteRelDataCallBackFLAG_NORMAL, this, iPfnContext);

                auto cpos = lseek(mCurStorFileFd, 0, SEEK_CUR);
                if (cpos != mCurFileFillSize) {
                    LOGERROR("Some error occured. current fd position isn't equal to mCurFileFillSize\n");
                }
                if (remainderSz != (uint64_t)write(mCurStorFileFd, iData + wtsz, remainderSz)) {
                    lseek(mCurStorFileFd, cpos, SEEK_SET);
                    LOGERROR("Fail to write remainded %lu real data to current store file\n", remainderSz);
                    return false;
                }

                mCurFileFillSize += remainderSz;

                if (usedNexStorFle) {
                    if (iPfn) iPfn(iData + wtsz, remainderSz, iCrossFiles, PFNWriteRelDataCallBackFLAG_AFTER_FIRST, this, iPfnContext);
                }
            }
        }

        return true;
    }

    bool PartitionFiles()
    {
        std::string tmFileName;
        FILE* fd{nullptr};
        int errNo{0};
        mStorFileEntryLst.clear();
        mFStorIndices.clear();
        for (uint32_t i = 0; i < mFileQuantities; i++) {
            tmFileName = mFStoreRootPath + "/" + std::to_string(i) + ".fstor";
            fd = fopen(tmFileName.c_str(), "w+");
            if (fd == nullptr) {
                LOGERROR("Fail to partition file; on folder: %s; at file no: %d\n",mFStoreRootPath.c_str(), i);
                return false;
            }

            if ((errNo = posix_fallocate64(fileno(fd), 0, mPerFileSzBytes)) != 0) {
                fclose(fd);
                LOGERROR("Fail to allocate file: %s; errno: %d\n", tmFileName.c_str(), errNo);
                return false;
            }

            if (0 != fclose(fd)) {
                LOGERROR("Fail to close file descriptor: %s\n", tmFileName.c_str());
                return false;
            }

            mStorFileEntryLst.push_back(tmFileName);
            mFStorIndices.push_back(FStoreMetaData());
        }

        fd = fopen((mFStoreRootPath + "/" + "indices").c_str(), "w+");
        if (fd == nullptr) {
            LOGERROR("Fail to generate indices file: %s\n", (mFStoreRootPath + "/" + "indices").c_str());
            return false;
        }

        if ((errNo = posix_fallocate64(fileno(fd), 0, sizeof(FStoreMetaData) * mFileQuantities)) != 0) {
            LOGERROR("Fail to allocate indices file: %s; errno: %d\n", (mFStoreRootPath + "/" + "indices").c_str(), errNo);
            return false;
        }

        fclose(fd);

        return mStorFileEntryLst.size() == mFileQuantities;
    }

    //the iOfstFromMetaDta must less than sizeof(FStoreMetaData::userData)
    bool WriteUserData(const uint8_t* iUserData, uint8_t iSz, uint32_t iOfstFromUsrDta)
    {
        if (!__writeUserData(iUserData, iSz, iOfstFromUsrDta, mCurStorFileFd)) {
            LOGERROR("Fail to write userdata for current store file \n");
            return false;
        }

        auto& fsMetaDataRef = mFStorIndices[mCurFileNo];
        memcpy((char*)(&fsMetaDataRef) + (sizeof(FStoreMetaData) - FStoreMetaDataUserDataLen) + iOfstFromUsrDta, iUserData, iSz);

        return true;
    }
    bool WriteLastStorFileUserData(const uint8_t* iUserData, uint8_t iSz, uint32_t iOfstFromUsrDta)
    {
        if (!__writeUserData(iUserData, iSz, iOfstFromUsrDta, mLastStorFileFd)) {
            LOGERROR("Fail to write userdata for last store file \n");
            return false;
        }

        auto& fsMetaDataRef = mFStorIndices[mCurFileNo > 0 ? mCurFileNo -1: mFileQuantities - 1];
        memcpy((char*)(&fsMetaDataRef) + (sizeof(FStoreMetaData) - FStoreMetaDataUserDataLen) + iOfstFromUsrDta, iUserData, iSz);

        return true;
    }

    //can use WriteMetaData to write userdata, because userdata is following with metadata
    //the iOfstFromMetaDta must less than sizeof(FStoreMetaData)
    bool WriteMetaData(const uint8_t* iMetaData, uint8_t iSz, uint32_t iOfstFromMetaDta)
    {
        if (!__writeMetaData(iMetaData, iSz, iOfstFromMetaDta, mCurStorFileFd)) {
            LOGERROR("Fail to write metadata for current store file \n");
            return false;
        }

        auto& fsMetaDataRef = mFStorIndices[mCurFileNo];
        memcpy((char*)(&fsMetaDataRef) + iOfstFromMetaDta, iMetaData, iSz);

        return true;
    }
    bool WriteLastStorFileMetaData(const uint8_t* iMetaData, uint8_t iSz, uint32_t iOfstFromMetaDta)
    {
        if (!__writeMetaData(iMetaData, iSz, iOfstFromMetaDta, mLastStorFileFd)) {
            LOGERROR("Fail to write metadata for last store file \n");
            return false;
        }

        auto& fsMetaDataRef = mFStorIndices[mCurFileNo > 0 ? mCurFileNo -1: mFileQuantities - 1];
        memcpy((char*)(&fsMetaDataRef) + iOfstFromMetaDta, iMetaData, iSz);

        return true;
    }

    const std::vector<FStoreMetaData>& fStorIndices() const
    {
        return mFStorIndices;
    }

    //indices is matching to each of store file's index
    bool ExistValidFS()
    {
        bool ret{true};
        struct stat statBuf;
        if (-1 == stat((mFStoreRootPath + "/" + "indices").c_str(), &statBuf)) {
            LOGERROR("Fail to call stat, errno %d\n", errno);
            return false;
        }

        int fileNo{0};
        while (0 == access((mFStoreRootPath + "/" + std::to_string(fileNo) + std::string(".fstor")).c_str(), R_OK | W_OK)) {
            fileNo++;
        }

        if (statBuf.st_size != (uint32_t) (fileNo * sizeof(FStoreMetaData))) {
            LOGERROR("Indices file's size isn't equal to the size calculated from fstor.indices sz %u calculated sz %llu. fileno %d\n",
                     (uint32_t)statBuf.st_size,
                     (uint32_t)(fileNo * sizeof(FStoreMetaData)),
                     fileNo);
            return false;
        }

        //    void *mmap(void *addr, size_t length, int prot, int flags,
        //                      int fd, off_t offset);
        //           int munmap(void *addr, size_t length);

        if (!openIndices()) {
            return false;
        }

        mIndicesMap = (char*)mmap(0, statBuf.st_size, PROT_READ|PROT_WRITE, MAP_SHARED, mIndicesFd, 0);
        if (mIndicesMap == MAP_FAILED) {
            LOGERROR("Fail to call mmap, errno %d\n", errno);
            return false;
        }

        mIndicesMapSz = statBuf.st_size;

        FStoreMetaData fsMetDtBuffer;
        auto szFsMetData = sizeof(FStoreMetaData);
        int fd{-1};
        for (int i = 0; i < fileNo; i++) {
            if ((fd = open((mFStoreRootPath + "/" + std::to_string(i) + std::string(".fstor")).c_str(), R_OK)) < 0 ) {
                LOGERROR("Fail to call open, fileno %d, errno %d\n", i, errno);
                ret = false;
                break;
            }

            if (-1 == lseek(fd,  -((__off_t)sizeof(FStoreMetaData)), SEEK_END)) {
                LOGERROR("Fail to call lseek, errno %d\n", errno);
                ret = false;
                close(fd);
                break;
            }

            if ((ssize_t)szFsMetData != read(fd, &fsMetDtBuffer, sizeof(FStoreMetaData))) {
                LOGERROR("Fail to call read, errno %d\n", errno);
                ret = false;
                close(fd);
                break;
            }

            if (0 != memcmp(&fsMetDtBuffer, ((FStoreMetaData*)mIndicesMap) + i, sizeof(FStoreMetaData))) {
                LOGINFO("indices doesn't match index of %d.fstor \n", i);
                ret = false;
                close(fd);
                break;
            }

            close(fd);
        }

        //    if (-1 == munmap(mIndicesMap, statBuf.st_size)) {
        //        LOGERROR("Fail to call munmap, errno %d\n", errno);
        //        return false;
        //    }

        return ret;
    }
private:
    //will restore fd's offset after that all of operations are finished
    bool __writeMetaData(const uint8_t* iMetaData, uint8_t iSz, uint32_t iOfstFromMetaDta, int fd)
    {
        auto orgPos = lseek(fd, 0, SEEK_CUR);
        if (-1 == orgPos) {
            LOGERROR("Fail to call lseek. errno %d\n", errno);
            return false;
        }

        bool ret{false};
        if (iSz == 0) {
            ret = true;
            goto end;
        }

        if (fd <= 0) {
            LOGERROR("fd %d <= 0; fd is less than or equal to 0\n", fd);
            goto end;
        }

        if (iSz + iOfstFromMetaDta > sizeof(FStoreMetaData)) {
            LOGERROR("iSz + iOfstFromMetaDta %u >= sizeof(FStoreMetaData)\n", iOfstFromMetaDta);
            goto end;
        }

        if (-1 == lseek(fd, - (int)sizeof(FStoreMetaData) - 1 +  (int)iOfstFromMetaDta, SEEK_END)) {
            LOGERROR("Fail to lseek %d SEEK_END, errno %d\n", (- (int)sizeof(FStoreMetaData) - 1 +  (int)iOfstFromMetaDta), errno);
            goto end;
        }

        if (iSz != write(fd, iMetaData, iSz)) {
            LOGERROR("Fail to write %u bytes meta data to store file, errno %d\n", iSz, errno);
            goto end;
        }

        ret = true;

end:
        if (-1 != lseek(fd, orgPos, SEEK_SET)) {
            LOGERROR("Fail to call lseek. errno %d\n", errno);
            return false;
        }

        return ret;
    }
    //will restore fd's offset after that all of operations are finished
    bool __writeUserData(const uint8_t* iUserData, uint8_t iSz, uint32_t iOfstFromUsrDta, int fd)
    {
        auto orgPos = lseek(fd, 0, SEEK_CUR);
        if (-1 == orgPos) {
            LOGERROR("Fail to call lseek. errno %d\n", errno);
            return false;
        }

        bool ret{false};
        if (iSz == 0) {
            ret = true;
            goto end;
        }


        if (fd <= 0) {
            LOGERROR("fd %d <= 0; fd is less than or equal to 0\n", fd);
            goto end;
        }

        if (iSz + iOfstFromUsrDta > FStoreMetaDataUserDataLen) {
            LOGERROR("iSz + iOfstFromUsrDta %u >= FStoreMetaDataUserDataLen (1000)\n", iOfstFromUsrDta);
            goto end;
        }

        if (-1 == lseek(fd, - (int)(FStoreMetaDataUserDataLen - 1) +  (int)iOfstFromUsrDta, SEEK_END)) {
            LOGERROR("Fail to lseek %d SEEK_END, errno %d\n", (- (int)(FStoreMetaDataUserDataLen - 1) +  (int)iOfstFromUsrDta), errno);
            goto end;
        }

        if (iSz != write(fd, iUserData, iSz)) {
            LOGERROR("Fail to write %u bytes meta data to store file, errno %d\n", iSz, errno);
            goto end;
        }

end:
        if (-1 != lseek(fd, orgPos, SEEK_SET)) {
            LOGERROR("Fail to call lseek. errno %d\n", errno);
            return false;
        }

        return ret;
    }
    bool useNextStoreFile()
    {
        if (mLastStorFileFd > 0) {
            close(mLastStorFileFd);
            //        mLastStorFileFd = 0;
        }

        mLastStorFileFd = mCurStorFileFd;

        mCurFileNo = (mCurFileNo + 1) % mFileQuantities;

        mCurStorFileFd = open(mStorFileEntryLst[mCurFileNo].c_str(), O_WRONLY);
        if (mCurStorFileFd < 0) {
            LOGERROR("Fail to open file %s to write. errno %d\n", mStorFileEntryLst[mCurFileNo].c_str(), errno);
        } else {
            LOGINFO("Using store file %s\n", mStorFileEntryLst[mCurFileNo].c_str());
        }

        mLastFileFillSize = mCurFileFillSize;
        mCurFileFillSize = 0;

        return mCurStorFileFd > 0;
    }

    /*
    *
    *  [0, mFileQuantities)
    *  0 <= iFrom <= iTo < mFileQuantities
    */
    bool syncMemToIndices(int iFrom, int iTo)
    {
        if (mIndicesFd <= 0) {
            LOGERROR("Indices's fd %d is invalid\n", mIndicesFd);
            return false;
        }

        //    if (-1 == lseek(mIndicesFd, iFrom * (__off_t)sizeof(FStoreMetaData), SEEK_SET)) {
        //        LOGERROR("Fail to call lseek, errno %d\n", errno);
        //        return false;
        //    }


        //    if(-1 == write(mIndicesFd, mFStorIndices.data() + iFrom, sizeof(FStoreMetaData) * (iTo - iFrom + 1))) {
        //        LOGERROR("Fail to call write, errno %d\n", errno);
        //        return false;
        //    }

        memcpy(mIndicesMap + sizeof(FStoreMetaData) * iFrom, mFStorIndices.data() + iFrom, sizeof(FStoreMetaData) * (iTo - iFrom + 1));
        if (msync(mIndicesMap + sizeof(FStoreMetaData) * iFrom, sizeof(FStoreMetaData) * (iTo - iFrom + 1), MS_ASYNC) != 0) {
            LOGERROR("Fail to msync. errno %d\n", errno);
            return false;
        }
        return true;
    }
    bool syncIndicesToMem()
    {
        auto offset = lseek(mIndicesFd, 0, SEEK_END);
        if (offset == -1) {
            LOGERROR("Fail to call lseek, errno %d\n", errno);
            return false;
        }

        if ((uint32_t)offset != mFileQuantities * sizeof(FStoreMetaData)) {
            return false;
        }

        lseek(mIndicesFd, 0, SEEK_SET);

        if ((ssize_t)offset != read(mIndicesFd, mFStorIndices.data(), offset)) {
            LOGERROR("Fail to call read, errno %d\n", mIndicesFd);
            return false;
        }

        return true;
    }

    bool openIndices()
    {
        if (mIndicesFd > 0) {
            close(mIndicesFd);
            mIndicesFd = 0;
        }

        mIndicesFd = open((mFStoreRootPath + "/" + "indices").c_str(), O_WRONLY);
        if (mIndicesFd < 0) {
            LOGERROR("Fail to open indices %s to write, errno: %d\n", (mFStoreRootPath + "/" + "indices").c_str(), errno);
            return false;
        }

        return true;
    }
    bool closeIndices()
    {
        if (mIndicesFd <= 0) {
            LOGERROR("Indices's fd %d is invalid\n", mIndicesFd);
            return false;
        }

        close(mIndicesFd);
        return true;
    }
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
    FStoreMetaData* mFStorIndicesPtr{nullptr};
    int mIndicesFd{-1};
    char* mIndicesMap{nullptr};
    uint32_t mIndicesMapSz{0};
};
#endif //FSTORE_H
