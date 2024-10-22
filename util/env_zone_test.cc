// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include "db/db_impl.h"
#include "leveldb/db.h"
#include "leveldb/env.h"
#include "util/testharness.h"
#include <string>
#include <vector>
#include <iostream>

using namespace std;

namespace leveldb {

class ZoneEnvTest {
 public:
  Env* env_;

  ZoneEnvTest()
     : env_((Env::Default())) {}
};
/*
TEST(ZoneEnvTest, DBTest) {
  Options options;
  options.create_if_missing = true;
  options.env = env_;
  DB* db;
  
  cout << "val" << endl;

  const Slice keys[] = {Slice("aaa"), Slice("bbb"), Slice("ccc")};
  const Slice vals[] = {Slice("foo"), Slice("bar"), Slice("baz")};

  cout << "key val" << endl;

  ASSERT_OK(DB::Open(options, "/dir/db", &db));
 
  cout << "db open" << endl;

  for (size_t i = 0; i < 3; ++i) {
    ASSERT_OK(db->Put(WriteOptions(), keys[i], vals[i]));
    cout << "put" << endl;
  }

  for (size_t i = 0; i < 3; ++i) {
    std::string res;
    ASSERT_OK(db->Get(ReadOptions(), keys[i], &res));
    ASSERT_TRUE(res == vals[i]);
  }

  Iterator* iterator = db->NewIterator(ReadOptions());
  iterator->SeekToFirst();
  for (size_t i = 0; i < 3; ++i) {
    ASSERT_TRUE(iterator->Valid());
    ASSERT_TRUE(keys[i] == iterator->key());
    ASSERT_TRUE(vals[i] == iterator->value());
    iterator->Next();
  }
  ASSERT_TRUE(!iterator->Valid());
  delete iterator;

  DBImpl* dbi = reinterpret_cast<DBImpl*>(db);
  ASSERT_OK(dbi->TEST_CompactMemTable());

  for (size_t i = 0; i < 3; ++i) {
    std::string res;
    ASSERT_OK(db->Get(ReadOptions(), keys[i], &res));
    ASSERT_TRUE(res == vals[i]);
  }

  delete db;
}
*/


TEST(ZoneEnvTest, WriteRead) {
    WritableFile* wf1;
    //WritableFile* wf2;
    //RandomAccessFile* rand_file;

    string str1 = "";
    string str2 = "";
    string str3 = "";
    string str4 = "";
    for(unsigned int i = 0; i<1000; i++)
		str1 += "A";
    for(unsigned int i = 0; i<2344; i++)
		str2 += "B";
    for(unsigned int i = 0; i<1123; i++)
		str3 += "C";
    for(unsigned int i = 0; i<302; i++)
		str4 += "D";
    
    ASSERT_OK(env_->CreateDir("/dir"));    

    ASSERT_OK(env_->NewWritableFile("/dir/f", &wf1));
    //ASSERT_OK(wf1->Append("hello"));
    //ASSERT_OK(wf1->Append("world"));
    ASSERT_OK(wf1->Append(str1));
    ASSERT_OK(wf1->Append(str2));
    //ASSERT_OK(wf1->Append(str3));
    //ASSERT_OK(wf1->Append(str4));
/*    
    ASSERT_OK(env_->NewWritableFile("/dir/z", &wf2));
    ASSERT_OK(wf2->Append(str4));
   */
    ASSERT_OK(wf1->Close());
    //ASSERT_OK(wf2->Close());
    
    delete wf1;
    //delete wf2;
/*
    SequentialFile* seq_file; 
    Slice result;
    string sc;

    // Read sequentially.
    ASSERT_OK(env_->NewSequentialFile("/dir/f", &seq_file));

    //sc.resize(512);
    //ASSERT_OK(seq_file->Read(512, &result, &sc[0])); // Read "hello".

//    sc.resize(1);
//    ASSERT_OK(seq_file->Read(1, &result, &sc[0])); // Read "hello".`

//    sc.resize(2);
//    ASSERT_OK(seq_file->Read(2, &result, &sc[0])); // Read "hello".`

    sc.resize(12);
    ASSERT_OK(seq_file->Read(12, &result, &sc[0])); // Read "hello".`
    cout << result.data() << " " << result.size() << endl;
    //ASSERT_EQ(0, result.compare("hello"));
    //sc.resize(600);
    //ASSERT_OK(seq_file->Read(600, &result, &sc[0])); // Read "hello".`
    //cout << result.data() << endl;
    //sc.resize(404);
    //ASSERT_OK(seq_file->Read(404, &result, &sc[0])); // Read "hello".`
    //cout << result.data() << endl;
    //sc.resize(300);
    //ASSERT_OK(seq_file->Read(300, &result, &sc[0])); // Read "hello".`
    //cout << result.data() << endl;
    

    ASSERT_OK(seq_file->Skip(1));

    ASSERT_OK(seq_file->Read(5, &result, sc1)); // Read "world".
    cout << sc1 << endl;
    cout << result.data() << endl;
    cout << result.size() << endl;
    ASSERT_EQ(0, result.compare("world"));
  */  
//    delete seq_file;
}


/*
TEST(MemEnvTest, Basics) {
  uint64_t file_size;
  WritableFile* writable_file;
  std::vector<std::string> children;

  ASSERT_OK(env_->CreateDir("/dir"));

  // Check that the directory is empty.
  ASSERT_TRUE(!env_->FileExists("/dir/non_existent"));
  ASSERT_TRUE(!env_->GetFileSize("/dir/non_existent", &file_size).ok());
  ASSERT_OK(env_->GetChildren("/dir", &children));
  ASSERT_EQ(0, children.size());
  //cout << "0" << endl;
  // Create a file.
  ASSERT_OK(env_->NewWritableFile("/dir/f", &writable_file));
  ASSERT_OK(env_->GetFileSize("/dir/f", &file_size));
  ASSERT_EQ(0, file_size);
  delete writable_file;
  //cout << "1" << endl;
  // Check that the file exists.
  ASSERT_TRUE(env_->FileExists("/dir/f"));
  ASSERT_OK(env_->GetFileSize("/dir/f", &file_size));
  ASSERT_EQ(0, file_size);
  ASSERT_OK(env_->GetChildren("/dir", &children));
  ASSERT_EQ(1, children.size());
  ASSERT_EQ("f", children[0]);
  //cout << "2" << endl;
  // Write to the file.
  ASSERT_OK(env_->NewWritableFile("/dir/f", &writable_file));
  ASSERT_OK(writable_file->Append("abc"));
  delete writable_file;
  //cout << "3" << endl;
  // Check that append works.
  ASSERT_OK(env_->NewAppendableFile("/dir/f", &writable_file));
  ASSERT_OK(env_->GetFileSize("/dir/f", &file_size));
  ASSERT_EQ(3, file_size);
  ASSERT_OK(writable_file->Append("hello"));
  delete writable_file;
  //cout << "4" << endl;
  // Check for expected size.
  ASSERT_OK(env_->GetFileSize("/dir/f", &file_size));
  ASSERT_EQ(8, file_size);
  //cout << "5" << endl;
  // Check that renaming works.
  ASSERT_TRUE(!env_->RenameFile("/dir/non_existent", "/dir/g").ok());
  ASSERT_OK(env_->RenameFile("/dir/f", "/dir/g"));
  ASSERT_TRUE(!env_->FileExists("/dir/f"));
  ASSERT_TRUE(env_->FileExists("/dir/g"));
  ASSERT_OK(env_->GetFileSize("/dir/g", &file_size));
  ASSERT_EQ(8, file_size);
  //cout << "6" << endl;
  // Check that opening non-existent file fails.
  SequentialFile* seq_file;
  RandomAccessFile* rand_file;
  ASSERT_TRUE(!env_->NewSequentialFile("/dir/non_existent", &seq_file).ok());
  ASSERT_TRUE(!seq_file);
  ASSERT_TRUE(!env_->NewRandomAccessFile("/dir/non_existent", &rand_file).ok());
  ASSERT_TRUE(!rand_file);
  //cout << "7" << endl;
  // Check that deleting works.
  ASSERT_TRUE(!env_->DeleteFile("/dir/non_existent").ok());
  ASSERT_OK(env_->DeleteFile("/dir/g"));
  ASSERT_TRUE(!env_->FileExists("/dir/g"));
  ASSERT_OK(env_->GetChildren("/dir", &children));
  ASSERT_EQ(0, children.size());
  ASSERT_OK(env_->DeleteDir("/dir"));
  //cout << "8" << endl;
}

TEST(MemEnvTest, ReadWrite) {
  WritableFile* writable_file;
  SequentialFile* seq_file;
  RandomAccessFile* rand_file;
  Slice result;
  char scratch[100];

  ASSERT_OK(env_->CreateDir("/dir"));
  string str = "A";
  for(unsigned int i = 0; i< 1023; i++)
  {
		str += "A";
  }

  ASSERT_OK(env_->NewWritableFile("/dir/f", &writable_file));
  ASSERT_OK(writable_file->Append("hello "));
  ASSERT_OK(writable_file->Append("world"));
  ASSERT_OK(writable_file->Append(str));
  delete writable_file;

  // Read sequentially.
  ASSERT_OK(env_->NewSequentialFile("/dir/f", &seq_file));
  ASSERT_OK(seq_file->Read(5, &result, scratch)); // Read "hello".
  cout << result.data() << endl;
  cout << scratch << endl;
  cout << result.size() << endl;
  ASSERT_EQ(0, result.compare("hello"));
  ASSERT_OK(seq_file->Skip(1));
  ASSERT_OK(seq_file->Read(5, &result, scratch)); // Read "world".
  cout << result.data() << endl;
  cout << scratch << endl;
  cout << result.size() << endl;
  ASSERT_EQ(0, result.compare("world"));
  //ASSERT_OK(seq_file->Read(1000, &result, scratch)); // Try reading past EOF.
  //ASSERT_EQ(0, result.size());
  ASSERT_OK(seq_file->Skip(10000)); // Try to skip past end of file.
  ASSERT_OK(seq_file->Read(1000, &result, scratch));
  ASSERT_EQ(0, result.size());
  delete seq_file;

  // Random reads.
  ASSERT_OK(env_->NewRandomAccessFile("/dir/f", &rand_file));
  ASSERT_OK(rand_file->Read(6, 5, &result, scratch)); // Read "world".
  ASSERT_EQ(0, result.compare("world"));
  ASSERT_OK(rand_file->Read(0, 5, &result, scratch)); // Read "hello".
  ASSERT_EQ(0, result.compare("hello"));
  ASSERT_OK(rand_file->Read(10, 2, &result, scratch)); // Read "dA".
  ASSERT_EQ(0, result.compare("dA"));

  // Too high offset.
  ASSERT_TRUE(!rand_file->Read(10000, 5, &result, scratch).ok());
  delete rand_file;
}

TEST(MemEnvTest, Locks) {
  FileLock* lock;

  // These are no-ops, but we test they return success.
  ASSERT_OK(env_->LockFile("some file", &lock));
  ASSERT_OK(env_->UnlockFile(lock));
}

TEST(MemEnvTest, Misc) {
  std::string test_dir;
  ASSERT_OK(env_->GetTestDirectory(&test_dir));
  ASSERT_TRUE(!test_dir.empty());

  WritableFile* writable_file;
  ASSERT_OK(env_->NewWritableFile("/a/b", &writable_file));

  // These are no-ops, but we test they return success.
  ASSERT_OK(writable_file->Sync());
  ASSERT_OK(writable_file->Flush());
  ASSERT_OK(writable_file->Close());
  delete writable_file;
}

TEST(MemEnvTest, LargeWrite) {
  const size_t kWriteSize = 300 * 1024;
  char* scratch = new char[kWriteSize * 2];

  std::string write_data;
  for (size_t i = 0; i < kWriteSize; ++i) {
    write_data.append(1, static_cast<char>(i));
  }

  WritableFile* writable_file;
  ASSERT_OK(env_->NewWritableFile("/dir/f", &writable_file));
  ASSERT_OK(writable_file->Append("foo"));
  ASSERT_OK(writable_file->Append(write_data));
  delete writable_file;

  SequentialFile* seq_file;
  Slice result;
  ASSERT_OK(env_->NewSequentialFile("/dir/f", &seq_file));
  ASSERT_OK(seq_file->Read(3, &result, scratch)); // Read "foo".
  ASSERT_EQ(0, result.compare("foo"));

  size_t read = 0;
  std::string read_data;
  while (read < kWriteSize) {
    ASSERT_OK(seq_file->Read(kWriteSize - read, &result, scratch));
    read_data.append(result.data(), result.size());
    read += result.size();
  }
  ASSERT_TRUE(write_data == read_data);
  delete seq_file;
  delete [] scratch;
}

TEST(MemEnvTest, DBTest) {
  Options options;
  options.create_if_missing = true;
  options.env = env_;
  DB* db;

  const Slice keys[] = {Slice("aaa"), Slice("bbb"), Slice("ccc")};
  const Slice vals[] = {Slice("foo"), Slice("bar"), Slice("baz")};

  ASSERT_OK(DB::Open(options, "/dir/db", &db));
  for (size_t i = 0; i < 3; ++i) {
    ASSERT_OK(db->Put(WriteOptions(), keys[i], vals[i]));
  }

  for (size_t i = 0; i < 3; ++i) {
    std::string res;
    ASSERT_OK(db->Get(ReadOptions(), keys[i], &res));
    ASSERT_TRUE(res == vals[i]);
  }

  Iterator* iterator = db->NewIterator(ReadOptions());
  iterator->SeekToFirst();
  for (size_t i = 0; i < 3; ++i) {
    ASSERT_TRUE(iterator->Valid());
    ASSERT_TRUE(keys[i] == iterator->key());
    ASSERT_TRUE(vals[i] == iterator->value());
    iterator->Next();
  }
  ASSERT_TRUE(!iterator->Valid());
  delete iterator;

  DBImpl* dbi = reinterpret_cast<DBImpl*>(db);
  ASSERT_OK(dbi->TEST_CompactMemTable());

  for (size_t i = 0; i < 3; ++i) {
    std::string res;
    ASSERT_OK(db->Get(ReadOptions(), keys[i], &res));
    ASSERT_TRUE(res == vals[i]);
  }

  delete db;
}
*/
}  // namespace leveldb

int main(int argc, char** argv) {
  return leveldb::test::RunAllTests();
}
