/*  The Clipboard Project - Cut, copy, and paste anything, anywhere, all from the terminal.
    Copyright (C) 2023 Jackson Huff and other contributors on GitHub.com
    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.*/
#include "../clipboard.hpp"

#if defined(_WIN32) || defined(_WIN64)
#include <fcntl.h>
#include <format>
#include <io.h>
#endif

#if defined(__linux__) || defined(__unix__) || defined(__APPLE__)
#include <sys/stat.h>
#include <sys/types.h>
#endif

namespace PerformAction {

void info() {
    stopIndicator();
    fprintf(stderr, "%s", formatMessage("[info]┍━┫ ").data());
    fprintf(stderr, clipboard_name_message().data(), clipboard_name.data());
    fprintf(stderr, "%s", formatMessage("[info] ┣").data());
    int columns = thisTerminalSize().columns - ((clipboard_name_message.rawLength() - 2) + clipboard_name.length() + 7);
    for (int i = 0; i < columns; i++)
        fprintf(stderr, "━");
    fprintf(stderr, "%s", formatMessage("┑[blank]\n").data());

    auto displayEndbar = [] {
        static auto total_cols = thisTerminalSize().columns;
        fprintf(stderr, "\033[%zdG%s\r", total_cols, formatMessage("[info]│[blank]").data());
    };

#if defined(__linux__) || defined(__APPLE__) || defined(__unix__)
    struct stat info;
    stat(path.string().data(), &info);
    std::string time(std::ctime(&info.st_ctime));
    std::erase(time, '\n');
    displayEndbar();
    fprintf(stderr, formatMessage("[info]│ Last changed [help]%s[blank]\n").data(), time.data());
#elif defined(__WIN32__) || defined(__WIN64__)
    fprintf(stderr, formatMessage("[info]│ Last changed [help]%s[blank]\n").data(), std::format("{}", fs::last_write_time(path)).data());
#endif
    displayEndbar();
    fprintf(stderr, formatMessage("[info]│ Stored in [help]%s[blank]\n").data(), path.string().data());
    displayEndbar();
    fprintf(stderr, formatMessage("[info]│ Persistent? [help]%s[blank]\n").data(), path.is_persistent ? "Yes" : "No");
    displayEndbar();
    fprintf(stderr, formatMessage("[info]│ Total entries: [help]%zu[blank]\n").data(), path.totalEntries());

    size_t total_clipboard_bytes = 0;
    for (const auto& entry : fs::recursive_directory_iterator(path))
        total_clipboard_bytes += entry.is_regular_file() ? fs::file_size(entry) : directoryOverhead(entry);
    displayEndbar();
    fprintf(stderr, formatMessage("[info]│ Total clipboard size: [help]%s[blank]\n").data(), formatBytes(total_clipboard_bytes).data());

    if (path.holdsRawData()) {
        displayEndbar();
        fprintf(stderr, formatMessage("[info]│ Content size: [help]%s[blank]\n").data(), formatBytes(fs::file_size(path.data.raw)).data());
        displayEndbar();
        fprintf(stderr, formatMessage("[info]│ Content type: [help]%s[blank]\n").data(), inferMIMEType(fileContents(path.data.raw)).value_or("(Unknown)").data());
    } else {
        size_t total_bytes = 0;
        size_t files = 0;
        size_t directories = 0;
        for (const auto& entry : fs::recursive_directory_iterator(path.data))
            total_bytes += entry.is_directory() ? directoryOverhead(entry) : fs::file_size(entry);
        displayEndbar();
        fprintf(stderr, formatMessage("[info]│ Content size: [help]%s[blank]\n").data(), formatBytes(total_bytes).data());
        for (const auto& entry : fs::directory_iterator(path.data))
            entry.is_directory() ? directories++ : files++;
        displayEndbar();
        fprintf(stderr, formatMessage("[info]│ Files: [help]%zu[blank]\n").data(), files);
        displayEndbar();
        fprintf(stderr, formatMessage("[info]│ Directories: [help]%zu[blank]\n").data(), directories);
    }

    if (!available_mimes.empty()) {
        displayEndbar();
        fprintf(stderr, "%s", formatMessage("[info]│ Available types from GUI: [help]").data());
        for (const auto& mime : available_mimes) {
            fprintf(stderr, "%s", mime.data());
            if (mime != available_mimes.back()) fprintf(stderr, ", ");
        }
        fprintf(stderr, "%s", formatMessage("[blank]\n").data());
    }
    displayEndbar();
    fprintf(stderr, formatMessage("[info]│ Content cut? [help]%s[blank]\n").data(), fs::exists(path.metadata.originals) ? "Yes" : "No");

    displayEndbar();
    fprintf(stderr, formatMessage("[info]│ Locked by another process? [help]%s[blank]\n").data(), path.isLocked() ? "Yes" : "No");

    if (path.isLocked()) {
        displayEndbar();
        fprintf(stderr, formatMessage("[info]│ Locked by process with pid [help]%s[blank]\n").data(), fileContents(path.metadata.lock).data());
    }

    displayEndbar();
    if (fs::exists(path.metadata.notes))
        fprintf(stderr, formatMessage("[info]│ Note: [help]%s[blank]\n").data(), fileContents(path.metadata.notes).data());
    else
        fprintf(stderr, "%s", formatMessage("[info]│ There is no note for this clipboard.[blank]\n").data());

    displayEndbar();
    if (path.holdsIgnoreRegexes()) {
        fprintf(stderr, "%s", formatMessage("[info]│ Ignore regexes: [help]").data());
        auto regexes = fileLines(path.metadata.ignore);
        for (const auto& regex : regexes) {
            fprintf(stderr, "%s", regex.data());
            if (regex != regexes.back()) fprintf(stderr, ", ");
        }
        fprintf(stderr, "%s", formatMessage("[blank]\n").data());
    } else
        fprintf(stderr, "%s", formatMessage("[info]│ There are no ignore regexes for this clipboard.[blank]\n").data());

    fprintf(stderr, "%s", formatMessage("[info]┕").data());
    auto cols = thisTerminalSize().columns;
    for (int i = 0; i < cols - 2; i++)
        fprintf(stderr, "━");
    fprintf(stderr, "%s", formatMessage("┙[blank]\n").data());
}

void infoJSON() {
    printf("{\n");

    printf("    \"name\": \"%s\",\n", clipboard_name.data());

#if defined(__linux__) || defined(__APPLE__) || defined(__unix__)
    struct stat info;
    stat(path.string().data(), &info);
    std::string time(std::ctime(&info.st_ctime));
    std::erase(time, '\n');
    printf("    \"lastChanged\": \"%s\",\n", time.data());
#elif defined(__WIN32__) || defined(__WIN64__)
    printf("    \"lastChanged\": \"%s\",\n", std::format("{}", fs::last_write_time(path)).data());
#endif

    printf("    \"path\": \"%s\",\n", path.string().data());
    printf("    \"isPersistent\": %s,\n", path.is_persistent ? "true" : "false");
    printf("    \"totalEntries\": %zu,\n", path.totalEntries());

    size_t total_clipboard_bytes = 0;
    for (const auto& entry : fs::recursive_directory_iterator(path))
        total_clipboard_bytes += entry.is_directory() ? directoryOverhead(entry) : entry.file_size();
    printf("    \"totalBytes\": %zu,\n", total_clipboard_bytes);

    if (path.holdsRawData()) {
        printf("    \"contentBytes\": %zu,\n", fs::file_size(path.data.raw));
        printf("    \"contentType\": \"%s\",\n", inferMIMEType(fileContents(path.data.raw)).value_or("(Unknown)").data());
    } else {
        size_t total_bytes = 0;
        size_t files = 0;
        size_t directories = 0;
        for (const auto& entry : fs::recursive_directory_iterator(path.data))
            total_bytes += entry.is_directory() ? directoryOverhead(entry) : entry.file_size();
        for (const auto& entry : fs::directory_iterator(path.data))
            entry.is_directory() ? directories++ : files++;
        printf("    \"bytes\": %zu,\n", total_bytes);
        printf("    \"files\": %zu,\n", files);
        printf("    \"directories\": %zu,\n", directories);
    }

    if (!available_mimes.empty()) {
        printf("    \"availableTypes\": [");
        for (const auto& mime : available_mimes)
            printf("\"%s\"%s", mime.data(), mime != available_mimes.back() ? ", " : "");
        printf("],\n");
    }

    printf("    \"contentCut\": %s,\n", fs::exists(path.metadata.originals) ? "true" : "false");

    printf("    \"locked\": %s,\n", path.isLocked() ? "true" : "false");
    if (path.isLocked()) printf("    \"lockedBy\": \"%s\",\n", fileContents(path.metadata.lock).data());

    if (fs::exists(path.metadata.notes))
        printf("    \"note\": \"%s\"\n", std::regex_replace(fileContents(path.metadata.notes), std::regex("\""), "\\\"").data());
    else
        printf("    \"note\": \"\"\n");

    if (path.holdsIgnoreRegexes()) {
        printf("    \"ignoreRegexes\": [");
        auto regexes = fileLines(path.metadata.ignore);
        for (const auto& regex : regexes)
            printf("\"%s\"%s", std::regex_replace(regex, std::regex("\""), "\\\"").data(), regex != regexes.back() ? ", " : "");
        printf("]\n");
    } else {
        printf("    \"ignoreRegexes\": []\n");
    }

    printf("}\n");
}

} // namespace PerformAction