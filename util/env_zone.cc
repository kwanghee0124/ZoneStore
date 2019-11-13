#include <iostream>
#include <limits>
#include <map>
#include <string>
#include <list>
#include <queue>
#include <algorithm>

#include <dirent.h>
#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <inttypes.h>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <set>
#include <thread>
#include <type_traits>

#include "leveldb/env.h"
#include "leveldb/slice.h"
#include "leveldb/status.h"
#include "port/port.h"
#include "port/thread_annotations.h"
#include "util/posix_logger.h"
#include "util/env_posix_test_helper.h"
#include "util/mutexlock.h"
#include "zns_controller/controller.h"

#define ZONE_DEV "/dev/nvme0n1"
#define ZNS_BLOCK_SIZE 512

using namespace std;

namespace leveldb {
	namespace {
		constexpr const size_t kWritableFileBufferSize = 65536;

		static Status ZoneError(const std::string& context, int err_number) {
			if (err_number == ENOENT) {
				return Status::NotFound(context, strerror(err_number));
			} else {
				return Status::IOError(context, strerror(err_number));
			}
		}

/*
class Limiter {
public:
// Limit maximum number of resources to |max_acquires|.
Limiter(int max_acquires) : acquires_allowed_(max_acquires) {}

Limiter(const Limiter&) = delete;
Limiter operator=(const Limiter&) = delete;

// If another resource is available, acquire it and return true.
// Else return false.
bool Acquire() {
int old_acquires_allowed =
acquires_allowed_.fetch_sub(1, std::memory_order_relaxed);

if (old_acquires_allowed > 0)
return true;

acquires_allowed_.fetch_add(1, std::memory_order_relaxed);
return false;
}

// Release a resource acquired by a previous call to Acquire() that returned
// true.
void Release() {
acquires_allowed_.fetch_add(1, std::memory_order_relaxed);
}

private:
// The number of available resources.
//
// This is a counter and is not tied to the invariants of any other class, so
// it can be operated on safely using std::memory_order_relaxed.
std::atomic<int> acquires_allowed_;
};
*/

//port::Mutex zone_mutex_;

// zone information
class File_Info
{
    public:
        File_Info(ssize_t zone_number, uint64_t write_point, bool flush)
            : zone_number_(zone_number),
            write_point_(write_point),
            flush_(flush)
        {
            wnblock_ = 0;
            
            wsize_ = 0;
            rsize_ = 0;
            
            rnblock_ = 0;
            roffset_ = 0;
        }

        ~File_Info() {}

        uint64_t get_next_wp() { return (write_point_ + wnblock_); }

        ssize_t zone_number_;
        uint64_t write_point_;
        size_t wnblock_;
        
        ssize_t wsize_;
        ssize_t rsize_;
        
        size_t rnblock_;
        ssize_t roffset_;
    
        bool flush_;
    private:
};

class Zone_Info
{
    public:
        Zone_Info()
        {
            write_point_ = 0;
            condition_ = 0;
        }
        Zone_Info(uint64_t write_point, ssize_t condition)
            : write_point_(write_point),
            condition_(condition) {}
        
        ~Zone_Info()
        {
            write_point_ = 0;
            condition_ = 0;
        }

        uint64_t write_point_;
        ssize_t condition_;
};

// zone init - coordinator
class Zone_Coordinator {
    public:
        Zone_Coordinator(map<string, vector<File_Info*>>& f2z_map)
            : f2z_map_(f2z_map)
        {
			Zone_device_init();
        }
        
        ~Zone_Coordinator()
        {
			//zns_finish(zone_info, zone_number);
            delete[] zinfo;
            delete zns_info;
		}
               
        ssize_t Write(string fname, const char* data, size_t size, bool flush)
        {
            write_lock.Lock();
			
            File_Info* finfo;
            ssize_t znum = 0;
    
            ssize_t buf_cnt = size/ZNS_BLOCK_SIZE;
            if((size%ZNS_BLOCK_SIZE) and flush) buf_cnt++; 
            ssize_t block_cnt = Block_alloc(buf_cnt);

            // Zone Open check
            if(!zone_list.size() or !block_cnt) {
                znum = Zone_alloc();
                zone_list.push_back(znum);
                block_cnt = Block_alloc(buf_cnt);
            } else {
                for(int i=0; i<zone_list.size(); i++) {
                    znum = zone_list[i];
                    break;
                }
            }

            ssize_t buf_size = ZNS_BLOCK_SIZE * block_cnt;
            char buf[buf_size];
            memset(buf, 0, buf_size);

            if(flush and block_cnt == buf_cnt) buf_size = size;
            memcpy(buf, data, buf_size);
            
            // File check
            if(f2z_map_.find(fname) == f2z_map_.end()
                    or f2z_map_[fname].back()->zone_number_ != znum
                    or f2z_map_[fname].back()->get_next_wp() != zinfo[znum].write_point_
                    or f2z_map_[fname].back()->flush_
                    or flush) {
                finfo = new File_Info(znum, zinfo[znum].write_point_, flush);
                f2z_map_[fname].push_back(finfo);
            } else {
                finfo = f2z_map_[fname].back(); 
            }
         
            uint64_t write_pos = finfo->write_point_ + finfo->wnblock_;
            zns_write(zns_info, buf, buf_size, znum, write_pos);
            zinfo[znum].write_point_ += block_cnt;
            //cout << "[Write] " << buf << " " << buf_size << endl;           
            finfo->wnblock_ += block_cnt;
            finfo->wsize_ += buf_size;
           
            write_lock.Unlock();
            return buf_size;
        }
        
        ssize_t Read(File_Info* finfo, char* scratch, size_t size)
        {
            ssize_t copy_size = 0;
           
            if((finfo->roffset_+size) > kWritableFileBufferSize)
                size = kWritableFileBufferSize - finfo->roffset_;

            ssize_t block_cnt = (finfo->roffset_+size)/ZNS_BLOCK_SIZE;
            if((finfo->roffset_+size) % ZNS_BLOCK_SIZE) block_cnt++; 
            ssize_t buf_size = ZNS_BLOCK_SIZE * block_cnt;
            char* buf = new char[buf_size];
            memset(buf, 0, buf_size);

            if(finfo->wnblock_ <= finfo->rnblock_) {
                strcpy(scratch, "");
                return 0;
            }

            uint64_t read_pos = finfo->write_point_ + finfo->rnblock_; 
            zns_read(zns_info, buf, buf_size, finfo->zone_number_, read_pos);
           
            if(size >= ((finfo->wsize_ - finfo->rsize_) - finfo->roffset_)) {
                copy_size = ((finfo->wsize_ - finfo->rsize_) - finfo->roffset_);
                finfo->rnblock_++;
            } else copy_size = size;
            
            buf += finfo->roffset_;
            memcpy(scratch, buf, copy_size);
            finfo->rsize_ += (finfo->roffset_ + copy_size);
            
            //cout << "[Read] " << scratch << " " << copy_size << endl;  
            finfo->rnblock_ += (finfo->roffset_+copy_size)/ZNS_BLOCK_SIZE;
            finfo->roffset_ = ZNS_BLOCK_SIZE - (buf_size - (finfo->roffset_+copy_size));
            if(finfo->roffset_ < 0 or finfo->roffset_ == ZNS_BLOCK_SIZE) finfo->roffset_ = 0;
                 
            return copy_size;
        }

	private:
		void Zone_device_init()
		{
            // Zone Control
            // Open, device_info
            zns_info = new zns_share_info();
            zns_init(ZONE_DEV, zns_info);

            // Get nr_zones / max 530
            nr_zones = zns_info->totalzones;

            // Zone Info
            zinfo = new Zone_Info[nr_zones];
            for(int i=0; i<nr_zones; i++) {
                zinfo[i].write_point_ = (zns_info->zone_list)[i].zone_entry.write_pointer;
                zinfo[i].condition_ = (zns_info->zone_list)[i].zone_entry.zone_condition;
            }
		}

	    void Open_zone(unsigned int znum)
		{
            // zone open
			zns_zone_open(zns_info, znum); 
		    zinfo[znum].condition_ = 0x3;
        }

		void Close_zone(unsigned int znum)
		{
			// zone finish
            zns_zone_finish(zns_info, znum);
            zinfo[znum].condition_ = 0xE;
		}
       
        ssize_t Block_alloc(ssize_t buf_cnt)
        {
            int znum = zone_list.front();
            uint64_t max_wp = 3532032;
            uint64_t now_wp = zinfo[znum].write_point_;
           
            if((now_wp + buf_cnt) < max_wp) return buf_cnt;
            else {
                //if(!(max_wp - now_wp) and (zone_info->zone_list)[znum].zone_entry.zone_condition == 0xE) {
                if(!(max_wp - now_wp)) {
                    Close_zone(znum);
                    zone_list.pop_front(); 
                }
                return (max_wp - now_wp);
            }
        }

        ssize_t Zone_alloc()
		{
            int znum = 0;

            for(int i=0; i<nr_zones; i++) {
                if(zinfo[i].condition_ == 0x1/*Empty*/) {
                    Open_zone(i);
                    znum = i;

                    break;
                }
            }

            return znum;
		}
       
        //Lock
        port::Mutex write_lock;

        Zone_Info* zinfo;
        zns_share_info* zns_info;
		deque<int> zone_list;
        int nr_zones;

        map<string, vector<File_Info*>>& f2z_map_;
        //multimap<unsigned int, unsigned int>* l2z_map_;
};

class ZoneSequentialFile final : public SequentialFile
{
	public:
	    ZoneSequentialFile(Zone_Coordinator *coordinator, vector<File_Info*>& finfo)
            : coordinator_(coordinator), finfo_(finfo)
        {
            iter_ = finfo_.begin();
        }

		~ZoneSequentialFile() override
        {
            /*reset read offset*/
            for(auto iter=finfo_.begin(); iter!=finfo_.end(); iter++) {
                (*iter)->rnblock_ = 0;
                (*iter)->roffset_ = 0;
                (*iter)->rsize_ = 0;
            }
        }

		Status Read(size_t n, Slice* result, char* scratch) override
        {
            //cout << "[Seq Read]" << endl;
			Status status;
            string *read = new string("");
            size_t size = n;

            while(size > 0) {
                char temp_str[size];
                memset(temp_str, 0, size);
                
                ssize_t read_size = coordinator_->Read(*iter_, temp_str, size);

                read->append(temp_str, read_size);
                size -= read_size;

                if((*iter_)->wnblock_ <= (*iter_)->rnblock_) {
                    iter_++;
                    if(iter_ == finfo_.end()) {
                        iter_--;
                        break;
                    }
                }
                if(read_size == 0) break; 
            }
            *result = Slice(read->data(), read->size());
            
            return status;
		}

	    Status Skip(uint64_t offset) override 
        {
            uint64_t find_offset = 0;
            
            for(auto iter=iter_; iter!=finfo_.end(); iter++) {
                find_offset += (*iter)->wsize_;
                if(find_offset > offset) {
                    iter_ = iter;
                    break;
                }

            }
            
            ssize_t check_offset = (*iter_)->wsize_ - (find_offset - offset);
            ssize_t offset_cnt = check_offset/ZNS_BLOCK_SIZE;
            ssize_t offset_size = check_offset - (ZNS_BLOCK_SIZE * offset_cnt);
            
            (*iter_)->roffset_ = offset_size;
            (*iter_)->rnblock_ = offset_cnt;
            (*iter_)->rsize_ = 0;

			return Status::OK();
		}

	private:
        Zone_Coordinator* coordinator_;
        vector<File_Info*>& finfo_;
        vector<File_Info*>::iterator iter_;
};

class ZoneRandomAccessFile final : public RandomAccessFile
{
	public:
	    ZoneRandomAccessFile(Zone_Coordinator* coordinator, vector<File_Info*>& finfo)
            : coordinator_(coordinator), finfo_(finfo)
        {}

		~ZoneRandomAccessFile() override
        {
            for(auto iter=finfo_.begin(); iter!=finfo_.end(); iter++) {
                (*iter)->rnblock_ = 0;
                (*iter)->roffset_ = 0;
                (*iter)->rsize_ = 0;
            }
        }

		Status Read(uint64_t offset, size_t n, Slice* result, char* scratch) const override
        {
            //cout << "[Random Read] " << n << endl;
            Status status;
            uint64_t find_offset = 0;
            
            //cout << "[Random Read] offset : " << offset << endl;
            vector<File_Info*>::iterator iter;
            for(iter=finfo_.begin(); iter!=finfo_.end(); iter++) {
                find_offset += (*iter)->wsize_;
                if(find_offset > offset) break;
            }
            
            ssize_t check_offset = (*iter)->wsize_ - (find_offset - offset);
            ssize_t offset_cnt = check_offset/ZNS_BLOCK_SIZE;
            ssize_t offset_size = check_offset - (ZNS_BLOCK_SIZE * offset_cnt);
            
            (*iter)->roffset_ = offset_size;
            (*iter)->rnblock_ = offset_cnt;
            (*iter)->rsize_ = 0;

            string *read = new string("");
            size_t size = n;
            while(size > 0) {
                char temp_str[size];
                memset(temp_str, 0, size);

                ssize_t read_size = coordinator_->Read(*iter, temp_str, size);
                
                read->append(temp_str, read_size);
                size -= read_size;
               
                //cout << "[Random Read] : " << (*iter)->wnblock_ << " " << (*iter)->rnblock_  << endl;
                if((*iter)->wnblock_ <= (*iter)->rnblock_) {
                    iter++;
                    if(iter == finfo_.end()) {
                        iter--;
                        break;
                    }
                    
                    (*iter)->roffset_ = 0;
                    (*iter)->rnblock_ = 0;
                    (*iter)->rsize_ = 0;
                }
                
                if(read_size == 0) break;
            }

            *result = Slice(read->data(), read->size());
            return status;
		}

	private:
		Zone_Coordinator* coordinator_;
        vector<File_Info*>& finfo_;
};

class ZoneWritableFile final : public WritableFile
{
	public:
		ZoneWritableFile(Zone_Coordinator *coordinator, string fname)
            : coordinator_(coordinator),
              fname_(fname),
              pos_(0),
              is_manifest_(IsManifest(fname)) {}

		~ZoneWritableFile() {
            Close(); 
        }

		Status Append(const Slice& data) override
        {
            size_t write_size = data.size();
            const char* write_data = data.data();

            size_t copy_size = std::min(write_size, kWritableFileBufferSize - pos_);
            memcpy(buf_ + pos_, write_data, copy_size);
            write_data += copy_size;
            write_size -= copy_size;
            pos_ += copy_size;
			if (write_size == 0)
				return Status::OK();
           
		    Status status = FlushBuffer(false);
			if (!status.ok())
				return status;

            if(write_size < kWritableFileBufferSize) {
                memcpy(buf_, write_data, write_size);
                pos_ = write_size;
                return Status::OK();
            }

			return WriteUnbuffered(write_data, write_size, false);
		}
		
        Status Close() override
        {
            Status status = FlushBuffer(true);
            
            return status;
		}
		
        Status Flush() override { return FlushBuffer(true); }

		Status Sync() override
		{
			/*
            Status status = SyncDirIfManifest();
			if (!status.ok()) {
				return status;
			}
            */
            
            Status status = FlushBuffer(true);
			if (!status.ok())
				return status;

			return status;
		}

	private:
        Status FlushBuffer(bool flush)
		{
            Status status = WriteUnbuffered(buf_, pos_, flush);
            pos_ = 0;

			return Status::OK();
		}
        
        Status WriteUnbuffered(const char* data, size_t size, bool flush)
		{
            while(size > 0) {
                ssize_t write_result = coordinator_->Write(fname_, data, size, flush);
                data += write_result;
                size -= write_result;

                if(size != 0 and size < ZNS_BLOCK_SIZE and !flush) {
                    Append(Slice(data, size));
                    size = 0;
                }
            }

            return Status::OK();
		}
        /*
        Status SyncDirIfManifest()
        {
			Status status;
			if (!is_manifest_) {
				return status;
			}

            status = Sync();
            if(!status.ok())
                status = ZoneError("Dir Sync Error", errno);

			return status;
		}
        */
        static std::string Dirname(const std::string& filename) {
			std::string::size_type separator_pos = filename.rfind('/');
			if (separator_pos == std::string::npos) {
				return std::string(".");
			}
			assert(filename.find('/', separator_pos + 1) == std::string::npos);

			return filename.substr(0, separator_pos);
		}

        static Slice Basename(const std::string& filename)
		{
			std::string::size_type separator_pos = filename.rfind('/');
			if (separator_pos == std::string::npos) {
				return Slice(filename);
			}
			assert(filename.find('/', separator_pos + 1) == std::string::npos);

			return Slice(filename.data() + separator_pos + 1,
					filename.length() - separator_pos - 1);
		}

		static bool IsManifest(const std::string& filename)
        {
			return Basename(filename).starts_with("MANIFEST");
		}

        Zone_Coordinator* coordinator_;

        char buf_[kWritableFileBufferSize];
        size_t pos_;
        const string fname_;

        const bool is_manifest_; // True if the file's name starts with MANIFEST.
};


class ZoneFileLock : public FileLock
{
	public:
		std::string name_;
};

class ZoneLockTable
{
	private:
		port::Mutex mu_;
		std::set<std::string> locked_files_ GUARDED_BY(mu_);
	public:
		bool Insert(const std::string& fname) LOCKS_EXCLUDED(mu_) {
			mu_.Lock();
			bool succeeded = locked_files_.insert(fname).second;
			mu_.Unlock();
			return succeeded;
		}
		void Remove(const std::string& fname) LOCKS_EXCLUDED(mu_) {
			mu_.Lock();
			locked_files_.erase(fname);
			mu_.Unlock();
		}
};


class ZoneEnv : public Env
{
	public:
        ZoneEnv();
		~ZoneEnv() override
        {
            f2z_map.clear();
            delete coordinator;
        }

		Status NewSequentialFile(const std::string& fname, SequentialFile** result) override
        {
			//MutexLock lock(&mutex_);
            if (f2z_map.find(fname) == f2z_map.end()) {
				*result = nullptr;
				return ZoneError(fname, errno);
			}
            
			*result = new ZoneSequentialFile(coordinator, f2z_map[fname]);
		    return Status::OK();
		}

		Status NewRandomAccessFile(const std::string& fname, RandomAccessFile** result) override
        {
			//MutexLock lock(&mutex_);
            if (f2z_map.find(fname) == f2z_map.end()) {
				*result = nullptr;
				return ZoneError(fname, errno);
			}
            
			*result = new ZoneRandomAccessFile(coordinator, f2z_map[fname]);
		    return Status::OK();
		}

		Status NewWritableFile(const std::string& fname, WritableFile** result) override
        {
			unsigned int level = 0;
           
            //MutexLock lock(&mutex_);
            
            // file find
            /*
            if (f2z_map.find(fname) != f2z_map.end()) {
				DeleteFile(fname);
			}
            */
            *result = new ZoneWritableFile(coordinator, fname);
			return Status::OK();
		}

		Status NewAppendableFile(const std::string& fname, WritableFile** result) override
        {
			unsigned int level = 0;

			//MutexLock lock(&mutex_);
		
            //if(f2z_map.find(fname))

            *result = new ZoneWritableFile(coordinator, fname);
			return Status::OK();
		}

		bool FileExists(const std::string& fname) override
        {
			//MutexLock lock(&mutex_);
            return f2z_map.find(fname) != f2z_map.end();
		}

		Status GetChildren(const std::string& dir, std::vector<std::string>* result) override
        {
			//MutexLock lock(&mutex_);
			result->clear();

            for(auto iter = f2z_map.begin(); iter != f2z_map.end(); iter++) {
                string fname = iter->first;

                if(dir.data() == fname.substr(0, dir.size())) {
                    result->emplace_back(fname.substr(dir.size() + 1));
                }
            }
            return Status::OK();
		}

		Status DeleteFile(const std::string& fname) override
		{
			//MutexLock lock(&mutex_);
			if (f2z_map.find(fname) == f2z_map.end()) {
				return ZoneError(fname, errno);
            }
           
            for(auto iter=f2z_map[fname].begin(); iter!=f2z_map[fname].end(); iter++)
                delete *iter;

            f2z_map.erase(fname);

			return Status::OK();
		}

		Status CreateDir(const std::string& dirname) override
		{
			return Status::OK();
		}

		Status DeleteDir(const std::string& dirname) override
		{
			return Status::OK();
		}

		Status GetFileSize(const std::string& fname, uint64_t* file_size) override
		{
            uint64_t fsize = 0;
			//MutexLock lock(&mutex_);
            if (f2z_map.find(fname) == f2z_map.end()) {
				return Status::IOError(fname, "File not found");
			}
            
            for(auto iter=f2z_map[fname].begin(); iter!=f2z_map[fname].end(); iter++) {
                fsize += (*iter)->wsize_;
            }
			*file_size = fsize;
            
            return Status::OK();
		}

		Status RenameFile(const std::string& src, const std::string& target) override
		{
			//MutexLock lock(&mutex_);
            if (f2z_map.find(src) == f2z_map.end()) {
				return Status::IOError(src, "File not found");
			}

			f2z_map[target] = f2z_map[src];
			f2z_map.erase(src);
            
            return Status::OK();
		}

		/*
		Status LockFile(const std::string& fname, FileLock** lock) override {
		    lock = new FileLock;
		    return Status::OK();
		}

		Status UnlockFile(FileLock* lock) override {
		    delete lock;
		    return Status::OK();
		}
		*/

		Status LockFile(const std::string& fname, FileLock** lock) override
		{
			*lock = nullptr;
			Status result;
		/*
			   int fd = open(fname.c_str(), O_RDWR | O_CREAT, 0644);
			   if (fd < 0) {
			   result = ZoneError(fname, errno);
			   } else 
		*/	  
			if (!locks_.Insert(fname))
			{
				result = Status::IOError("lock " + fname, "already held by process");
			}
			/*
			   else if (LockOrUnlock(true) == -1)
			   {
			   result = ZoneError("lock " + fname, errno);
			   locks_.Remove(fname);
			   }
			   */
			else
			{
				ZoneFileLock* my_lock = new ZoneFileLock;
				my_lock->name_ = fname;
				*lock = my_lock;
			}

			return result;
		}

		Status UnlockFile(FileLock* lock) override
		{
			ZoneFileLock* my_lock = reinterpret_cast<ZoneFileLock*>(lock);
			Status result;
			/*
			   if (LockOrUnlock(false) == -1) {
			   result = ZoneError("unlock", errno);
			   }
			*/
			locks_.Remove(my_lock->name_);

			//delete my_lock;
			return result;
		}

		void Schedule(void (*background_work_function)(void* background_work_arg), void* background_work_arg) override
		{
			background_work_mutex_.Lock();

			if (!started_background_thread_)
			{
				started_background_thread_ = true;
				std::thread background_thread(ZoneEnv::BackgroundThreadEntryPoint, this);
				background_thread.detach();
			}

			if (background_work_queue_.empty())
			{
				background_work_cv_.Signal();
			}
            
			background_work_queue_.emplace(background_work_function, background_work_arg);
			background_work_mutex_.Unlock();
		}

		void StartThread(void (*thread_main)(void* thread_main_arg), void* thread_main_arg) override
		{
			std::thread new_thread(thread_main, thread_main_arg);
			new_thread.detach();
		}

		Status GetTestDirectory(std::string* path) override
		{
			*path = "/test";
			return Status::OK();
		}

		Status NewLogger(const std::string& fname, Logger** result) override
		{
			//*result = new NoOpLogger;
			return Status::OK();
		}

	    uint64_t NowMicros() override
		{
            static constexpr uint64_t kUsecondsPerSecond = 1000000;
			struct timeval tv;
			::gettimeofday(&tv, nullptr);
			return static_cast<uint64_t>(tv.tv_sec) * kUsecondsPerSecond + tv.tv_usec;
		}

		void SleepForMicroseconds(int micros) override
		{
            std::this_thread::sleep_for(std::chrono::microseconds(micros));
		}

	private:
		void BackgroundThreadMain()
		{
			while (true) {
				background_work_mutex_.Lock();

				// Wait until there is work to be done.
				while (background_work_queue_.empty()) {
					background_work_cv_.Wait();
				}

				assert(!background_work_queue_.empty());
				auto background_work_function = background_work_queue_.front().function;
				void* background_work_arg = background_work_queue_.front().arg;
				background_work_queue_.pop();

				background_work_mutex_.Unlock();
				background_work_function(background_work_arg);
			}
		}

		static void BackgroundThreadEntryPoint(ZoneEnv* env)
		{
			env->BackgroundThreadMain();
		}

		// Stores the work item data in a Schedule() call.
		//
		// Instances are constructed on the thread calling Schedule() and used on the
		// background thread.
		//
		// This structure is thread-safe beacuse it is immutable.
		struct BackgroundWorkItem
		{
			explicit BackgroundWorkItem(void (*function)(void* arg), void* arg)
				: function(function), arg(arg) {}

			void (* const function)(void*);
			void* const arg;
		};

        
        // Zone Coordinator
        Zone_Coordinator *coordinator;
        
		// Need to seperate Lock
		port::Mutex mutex_;
        // F2Z_Map
        map<string, vector<File_Info*>> f2z_map GUARDED_BY(mutex_);
        //FileZoneMap file_map GUARDED_BY(mutex_);
        /*typedef map<std::string, ZoneFile*> ZoneFileMap;
        ZoneFileMap file_map_ GUARDED_BY(mutex_);*/

        // L2Z_Map
        //std::multimap<unsigned int, unsigned int> l2z_map;

        // Default
        port::Mutex background_work_mutex_;
		port::CondVar background_work_cv_ GUARDED_BY(background_work_mutex_);
		bool started_background_thread_ GUARDED_BY(background_work_mutex_);

		std::queue<BackgroundWorkItem> background_work_queue_
			GUARDED_BY(background_work_mutex_);
        //
        // LockTable 수정 필요
		ZoneLockTable locks_;
};

ZoneEnv::ZoneEnv()
	: background_work_cv_(&background_work_mutex_),
	started_background_thread_(false) {
            //coordinator = new Zone_Coordinator(f2z_map, l2z_map); 
            coordinator = new Zone_Coordinator(f2z_map); 
    }

	// Wraps an Env instance whose destructor is never created.
	//
	// Intended usage:
	//   using PlatformSingletonEnv = SingletonEnv<PlatformEnv>;
	//   void ConfigurePosixEnv(int param) {
	//     PlatformSingletonEnv::AssertEnvNotInitialized();
	//     // set global configuration flags.
	//   }
	//   Env* Env::Default() {
	//     static PlatformSingletonEnv default_env;
	//     return default_env.env();
	//   }
	template<typename EnvType>
	class SingletonEnv
{
	public:
		SingletonEnv() {
#if !defined(NDEBUG)
			env_initialized_.store(true, std::memory_order::memory_order_relaxed);
#endif  // !defined(NDEBUG)
			static_assert(sizeof(env_storage_) >= sizeof(EnvType),
					"env_storage_ will not fit the Env");
			static_assert(alignof(decltype(env_storage_)) >= alignof(EnvType),
					"env_storage_ does not meet the Env's alignment needs");
			new (&env_storage_) EnvType();
		}
		~SingletonEnv() = default;

		SingletonEnv(const SingletonEnv&) = delete;
		SingletonEnv& operator=(const SingletonEnv&) = delete;

		Env* env() { return reinterpret_cast<Env*>(&env_storage_); }

		static void AssertEnvNotInitialized() {
#if !defined(NDEBUG)
			assert(!env_initialized_.load(std::memory_order::memory_order_relaxed));
#endif  // !defined(NDEBUG)
		}

	private:
		typename std::aligned_storage<sizeof(EnvType), alignof(EnvType)>::type
			env_storage_;
#if !defined(NDEBUG)
		static std::atomic<bool> env_initialized_;
#endif  // !defined(NDEBUG)
};

#if !defined(NDEBUG)
template<typename EnvType>
std::atomic<bool> SingletonEnv<EnvType>::env_initialized_;
#endif  // !defined(NDEBUG)

using ZoneDefaultEnv = SingletonEnv<ZoneEnv>;

}  // namespace

Env* Env::Default() {
	static ZoneDefaultEnv env_container;
	return env_container.env();
}

}  // namespace leveldb
