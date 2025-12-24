#ifndef MULTIPART_PROCESSOR_H
#define MULTIPART_PROCESSOR_H

#include <iostream>
#include <string>
#include <string_view>
#include <vector>
#include <algorithm>

#include "../../../include/pool/io_task_pool.h"

namespace gee {
    class MultipartProcessor {
    public:
        enum class State {
            FIND_BOUNDARY,
            READ_PART_HEADER,
            READ_PART_BODY,
            FINISHED
        };

        /**
         * @param boundary 从 Content-Type 中提取的原始 boundary 字符串
         * @param save_path 文件保存目录，需以 / 结尾
         * @param pool 外部传入的 IO 线程池指针
         */
        MultipartProcessor(const std::string &boundary, const std::string &save_path, IOTaskPool *pool)
            : boundary_("\r\n--" + boundary),
              state_(State::FIND_BOUNDARY),
              save_path_(save_path),
              io_pool_(pool) {
            // 兼容第一笔数据可能没有 \r\n 的情况
            first_boundary_ = "--" + boundary;
        }

        ~MultipartProcessor() = default;

        // 获取最终保存的所有文件完整路径
        const std::vector<std::string> &get_saved_filenames() const {
            return saved_filenames_;
        }

        /**
         * 核心流式处理函数：将网络数据切片并投递到 IO 线程池
         */
        bool feed(std::string_view chunk) {
            buffer_.append(chunk.data(), chunk.size());

            while (!buffer_.empty()) {
                switch (state_) {
                    case State::FIND_BOUNDARY: {
                        auto pos = buffer_.find(boundary_);
                        size_t b_len = boundary_.size();
                        if (pos == std::string::npos && buffer_.find(first_boundary_) == 0) {
                            pos = 0;
                            b_len = first_boundary_.size();
                        }

                        if (pos != std::string::npos) {
                            buffer_.erase(0, pos + b_len);
                            state_ = State::READ_PART_HEADER;
                        } else {
                            keep_tail_for_safety();
                            return true;
                        }
                        break;
                    }

                    case State::READ_PART_HEADER: {
                        auto pos = buffer_.find("\r\n\r\n");
                        if (pos != std::string::npos) {
                            std::string_view header_part(buffer_.data(), pos);
                            parse_part_header(header_part);
                            buffer_.erase(0, pos + 4);

                            state_ = State::READ_PART_BODY;
                        } else {
                            // 数据不足以解析 Header，保留并等待下一块
                            return true;
                        }
                        break;
                    }

                    case State::READ_PART_BODY: {
                        auto pos = buffer_.find(boundary_);
                        std::string full_path = save_path_ + current_filename_;

                        if (pos != std::string::npos) {
                            // 1. 找到了下一个边界，提交该文件的最后一块数据，并触发 fsync
                            if (pos > 0) {
                                io_pool_->addTask(full_path, std::string(buffer_.data(), pos), true);
                            }

                            saved_filenames_.push_back(full_path);
                            buffer_.erase(0, pos);
                            state_ = State::FIND_BOUNDARY;
                        } else {
                            // 2. 没找到边界，将安全长度以外的数据异步刷入线程池
                            if (buffer_.size() > boundary_.size()) {
                                size_t write_len = buffer_.size() - boundary_.size();

                                // 这里必须进行 string 拷贝，因为 buffer_ 马上要被 erase
                                io_pool_->addTask(full_path, std::string(buffer_.data(), write_len), false);

                                buffer_.erase(0, write_len);
                            }
                            return true;
                        }
                        break;
                    }
                }
            }
            return true;
        }

        void finish() {
            // 如果 buffer_ 中还有残留数据（通常是最后一个 boundary 之后的内容）
            // 或者仅仅是为了确保之前的异步写入全部落盘
            if (!current_filename_.empty()) {
                std::string full_path = save_path_ + current_filename_;

                // 投递一个空数据任务，但 should_fsync 设为 true
                // 这样 IO 线程在写完之前的队列后，会执行一次 fsync
                io_pool_->addTask(full_path, "", true);

                // 记录文件名（如果最后一块还没记录的话）
                if (std::find(saved_filenames_.begin(), saved_filenames_.end(), full_path) == saved_filenames_.end()) {
                    saved_filenames_.push_back(full_path);
                }
            }
        }

    private:
        // 防止边界字符串被分片切断，保留末尾一段
        void keep_tail_for_safety() {
            if (buffer_.size() > boundary_.size()) {
                buffer_.erase(0, buffer_.size() - boundary_.size());
            }
        }

        void parse_part_header(std::string_view header) {
            auto pos = header.find("filename=\"");
            if (pos != std::string_view::npos) {
                auto start = pos + 10;
                auto end = header.find("\"", start);
                std::string raw_name = std::string(header.substr(start, end - start));

                auto last_slash = raw_name.find_last_of("/\\");
                if (last_slash != std::string::npos) {
                    current_filename_ = raw_name.substr(last_slash + 1);
                } else {
                    current_filename_ = raw_name;
                }
            } else {
                current_filename_ = "upload_" + std::to_string(rand() % 10000);
            }
        }

        std::string boundary_;
        std::string first_boundary_;
        std::string buffer_;
        State state_;
        std::string save_path_;
        std::string current_filename_;
        std::vector<std::string> saved_filenames_;

        // IO 线程池指针，由外部 Request 对象在 handle_multipart_streaming 中传入
        IOTaskPool *io_pool_;
    };
} // namespace gee

#endif // MULTIPART_PROCESSOR_H
