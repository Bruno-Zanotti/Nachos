#include "path.hh"

#include <string.h>

void
Path::Merge(const char* subpath)
{
    std::string sub(subpath);
    if (sub[0] == '/')
        path.clear();

    char* token = strtok(&sub[0], "/");
    while(token != NULL) {
        std::string part(token);
        if (part == ".") {
            // Same folder.
        } else if (part == "..") {
            if (path.size() > 0)
                path.pop_back();
        } else {
            path.push_back(part);
        }
        token = strtok(NULL, "/");
    }
}

std::string
Path::GetPath()
{
    std::string p;
    for (auto& i : path)
        p += "/" + i;
    return p;
}

std::string
Path::Split()
{
    std::string file = path.back();
    path.pop_back();
    return file;
}