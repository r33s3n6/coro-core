#ifndef TEST_NFS_SHELL_HPP
#define TEST_NFS_SHELL_HPP

#include <test/test.h>
#include <task_scheduler.h>
#include <utils/wait_queue.h>

#include <mm/allocator.h>

#include <fs/inode.h>
#include <fs/nfs/nfs.h>
#include <fs/nfs/inode.h>
#include <device/block/bdev.h>

namespace test {

namespace nfs {

#define CO_AWAIT_NOFAIL(t) \
    do { \
        auto _t = t; \
        _t.get_promise()->has_error_handler = true; \
        co_await _t; \
    } while (0)

#define CSTR(s) std::string(s).c_str()

class test_shell : public test_base {

    private:
    
    ::nfs::nfs fs;
    block_device *bdev;
    uint64 nblocks;

    shared_ptr<dentry> cwd;


    void get_line(std::string& line) {
        char c;
        while (true) {
            c = kernel_console_logger.getc();

            if (c == 127 || c == '\b') { // backspace
                if (line.size() > 0) {
                    line.pop_back();
                    _rawf("\b \b");
                }
                continue;
            } else if (c == '\r' || c == '\n') {
                sbi_console_putchar('\n');
                break;
            } else {
                kernel_console_logger.putc(c);
            }

            line += c;
            if (line.size() > 256) {
                rawf("line too long: %s", line.data());
                line = "";
                break;
            }
        }
    }

    std::string_view get_token(std::string_view& line) {
        while (line.size() > 0 && line[0] == ' ') {
            line.remove_prefix(1);
        }
        auto pos = line.find(' ');
        if (pos == std::string_view::npos) {
            auto ret = line;
            line.remove_prefix(line.size());
            return ret;
        }
        auto ret = line.substr(0, pos);
        line.remove_prefix(pos);
        return ret;
    }

    task<shared_ptr<dentry>> get_dentry(std::string_view& line) {
        if (!cwd) {
            rawf("fs not mounted");
            co_return nullptr;
        }
        auto dir = cwd;
        if (line.size() > 0) {
            auto path = get_token(line);
            dir = *co_await kernel_dentry_cache.get_at(dir, path);
            if (!dir) {
                rawf("cannot access '%s': No such file or directory", CSTR(path));
                co_return nullptr;
            }
        }

        co_return dir;
    }

    task<void> do_ls(std::string_view& line) {
        
        auto dir = *co_await get_dentry(line);
        if (!dir) {
            co_return task_ok;
        }
        
        {
            auto dir_inode = dir->get_inode();
            auto dir_iterator = dir_inode->read_dir();

            rawf("name inode size nlinks perm type");
            int count = 0;
            while (auto _dentry = *co_await dir_iterator) {
                auto _inode = _dentry->get_inode();
                auto _inode_ref = *co_await _inode->get_ref();
                auto metadata = *co_await _inode_ref->get_metadata();

                rawf("%s %d %d %d %d %d", _dentry->name.data(),
                       _inode_ref->inode_number, metadata->size, metadata->nlinks, metadata->perm, metadata->type);
                count++;
            }
            rawf("%d entries", count);


        }
        
    }

    task<void> do_mkfs() {
        bool success = co_await ::nfs::nfs::make_fs(bdev->device_id, nblocks);
        if (!success) {
            rawf("mkfs failed");
            co_return task_ok;
        }
        rawf("mkfs success");
        co_return task_ok;
        
    }

    task<void> do_mount() {
        bool success = co_await fs.mount(bdev->device_id);
        if (!success) {
            rawf("mount failed");
            co_return task_ok;
        }
        cwd = *co_await kernel_dentry_cache.get_at(nullptr, "/");
        rawf("mount success");
        co_return task_ok;
        
    }

    task<void> do_unmount() {
        cwd = nullptr;
        bool success = co_await fs.unmount();
        if (!success) {
            rawf("unmount failed");
            co_return task_ok;
        }

        rawf("unmount success");
        co_return task_ok;
        
    }

    task<void> do_cat(std::string_view& line) {
        auto dir = *co_await get_dentry(line);
        if (!dir) {
            co_return task_ok;
        }

        auto inode = dir->get_inode();
        char* buf;
        uint32 size;
        {
            auto inode_ref = *co_await inode->get_ref();
            auto metadata = *co_await inode_ref->get_metadata();
            if (metadata->type != inode::ITYPE_FILE) {
                rawf("cat: %s: Not a file", dir->name.data());
                co_return task_ok;
            }
            size = metadata->size;
            
        }
        buf = new char[size + 1];
        buf[size] = '\0';

        {
            simple_file sf(inode);
            co_await sf.open();
            co_await sf.read(buf, size);
            co_await sf.close();
        }

        rawf("%s", buf);
        delete buf;

        co_return task_ok;
    }

    std::string get_real_content(std::string_view& line) {
        auto content = get_token(line);
        std::string real_content;
        for (std::size_t i = 0; i < content.size(); i++) {
            if (content[i] == '\\') {
                i++;
                if (i == content.size()) {
                    real_content.push_back('\\');
                    break;
                } else if (content[i] == 'n') {
                    real_content.push_back('\n');
                } else if (content[i] == 't') {
                    real_content.push_back('\t');
                } else {
                    real_content.push_back(content[i]);
                }
            } else {
                real_content.push_back(content[i]);
            }
        }
        return real_content;
    }

    task<void> do_append(std::string_view& line) {
        auto dir = *co_await get_dentry(line);
        if (!dir) {
            co_return task_ok;
        }

        auto inode = dir->get_inode();
        {
            auto inode_ref = *co_await inode->get_ref();
            auto metadata = *co_await inode_ref->get_metadata();
            if (metadata->type != inode::ITYPE_FILE) {
                rawf("cat: %s: Not a file", dir->name.data());
                co_return task_ok;
            }

            
        }

        auto real_content = get_real_content(line);

        {
            simple_file sf(inode);
            co_await sf.open();
            co_await sf.llseek(0, file::whence_t::END);
            co_await sf.write(real_content.data(), real_content.size());
            co_await sf.close();
        }



        co_return task_ok;
    }

    task<void> do_write(std::string_view& line) {
        auto dir = *co_await get_dentry(line);
        if (!dir) {
            co_return task_ok;
        }

        auto inode = dir->get_inode();
        {
            auto inode_ref = *co_await inode->get_ref();
            auto metadata = *co_await inode_ref->get_metadata();
            if (metadata->type != inode::ITYPE_FILE) {
                rawf("cat: %s: Not a file", dir->name.data());
                co_return task_ok;
            }

            
        }

        auto real_content = get_real_content(line);

        {
            simple_file sf(inode);
            co_await sf.open();
            co_await sf.write(real_content.data(), real_content.size());
            co_await sf.close();
        }



        co_return task_ok;
    }

    task<void> do_touch(std::string_view& line) {
        if (!cwd) {
            rawf("fs not mounted");
            co_return task_ok;
        }
        // create file
        auto file_name = get_token(line);
        auto file_dentry = *co_await kernel_dentry_cache.create(cwd, file_name, nullptr);

        if (!file_dentry) {
            rawf("touch: %s: File exists", CSTR(file_name));
            co_return task_ok;
        }

        co_await cwd->get_inode()->create(file_dentry);
    }

    task<void> do_mkdir(std::string_view& line) {
        if (!cwd) {
            rawf("fs not mounted");
            co_return task_ok;
        }
        // create dir
        auto file_name = get_token(line);
        auto file_dentry = *co_await kernel_dentry_cache.create(cwd, file_name, nullptr);

        if (!file_dentry) {
            rawf("mkdir: %s: File exists", CSTR(file_name));
            co_return task_ok;
        }


        co_await cwd->get_inode()->mkdir(file_dentry);
    }

    task<void> do_unlink(std::string_view& line) {
        // remove file
        auto _dentry = *co_await get_dentry(line);

        if (!_dentry) {
            co_return task_ok;
        }

        co_await cwd->get_inode()->unlink(_dentry);
    }

    task<void> do_link(std::string_view& line) {
        // link file
        auto src_dentry = *co_await get_dentry(line);
        
        if (!src_dentry) {
            co_return task_ok;
        }

        auto file_name = get_token(line);
        auto file_dentry = *co_await kernel_dentry_cache.create(cwd, file_name, nullptr);

        if (!file_dentry) {
            rawf("link: %s: File exists", CSTR(file_name));
            co_return task_ok;
        }

        co_await cwd->get_inode()->link(src_dentry, file_dentry);
    }

    task<void> do_cd(std::string_view& line) {
        // change dir
        auto _dentry = *co_await get_dentry(line);

        if (!_dentry) {
            co_return task_ok;
        }

        auto inode = _dentry->get_inode();
        auto inode_ref = *co_await inode->get_ref();
        auto metadata = *co_await inode_ref->get_metadata();
        if (metadata->type != inode::ITYPE_DIR) {
            rawf("cd: %s: Not a directory", _dentry->name.data());
            co_return task_ok;
        }

        cwd = _dentry;
    }

    task<void> test_bigfile(std::string_view& line) {
        auto file_name = get_token(line);
        auto file_dentry = *co_await kernel_dentry_cache.create(cwd, file_name, nullptr);

        if (!file_dentry) {
            rawf("touch: %s: File exists", CSTR(file_name));
            co_return task_ok;
        }

        co_await cwd->get_inode()->create(file_dentry);

        const char* content = "hello world, 1234567890abcdefghijklmnopqrstuvwxyz12345678901234\n";


        {
            simple_file sf(file_dentry->get_inode());
            co_await sf.open();
            for (int i=0; i < 1024 * 16; i++)
                co_await sf.write(content, 64);
            co_await sf.close();
        }

    }


    task<void> shell() {

        while (true) {
            std::string line;
            _rawf("nfs> ");
            get_line(line);
            if (line.size() == 0) continue;
            std::string_view line_view(line);

            auto cmd = get_token(line_view);

                 if (cmd == "ls")      CO_AWAIT_NOFAIL(do_ls(line_view));
            else if (cmd == "mkfs")    CO_AWAIT_NOFAIL(do_mkfs());
            else if (cmd == "mount")   CO_AWAIT_NOFAIL(do_mount());
            else if (cmd == "unmount") CO_AWAIT_NOFAIL(do_unmount());
            else if (cmd == "cat")     CO_AWAIT_NOFAIL(do_cat(line_view));
            else if (cmd == "append")  CO_AWAIT_NOFAIL(do_append(line_view));
            else if (cmd == "write")   CO_AWAIT_NOFAIL(do_write(line_view));
            else if (cmd == "touch")   CO_AWAIT_NOFAIL(do_touch(line_view));
            else if (cmd == "mkdir")   CO_AWAIT_NOFAIL(do_mkdir(line_view));
            else if (cmd == "unlink")  CO_AWAIT_NOFAIL(do_unlink(line_view));
            else if (cmd == "link")    CO_AWAIT_NOFAIL(do_link(line_view));
            else if (cmd == "cd")      CO_AWAIT_NOFAIL(do_cd(line_view));
            else if (cmd == "test_bigfile")      CO_AWAIT_NOFAIL(test_bigfile(line_view));
            else if (cmd == "help")    rawf("Commands: ls, mkfs, mount, unmount, cat, append, write, touch, mkdir, unlink, link, cd, help, exit");
                
            else if (cmd == "exit") break;
            else rawf("Unknown command: %s", CSTR(cmd));

            
        }
        




        
        real_test_lock.lock();
        real_test_done = true;
        real_test_lock.unlock();

        real_test_wait_queue.wake_up();

        co_return task_ok;
    }

    private:
    single_wait_queue real_test_wait_queue;
    spinlock real_test_lock;
    bool real_test_done = false;


    public:
    test_shell(block_device *bdev, uint64 nblocks): bdev(bdev), nblocks(nblocks) {
    }

    bool run() override {

        push_task(shell());


        real_test_lock.lock();

        while (!real_test_done) {
            cpu::my_cpu()->sleep(&real_test_wait_queue, real_test_lock);
        }

        real_test_lock.unlock();
        
        return true;
    }
    void print() {
        
    }

    private:



}; // class test_shell

} // namespace nfs

} // namespace test

#endif