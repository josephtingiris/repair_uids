#include <algorithm>
#include <fstream>
#include <sstream>
#include <string>

#include "iddb.h"
#include "string_utils.h"

using namespace std;

class IdEntry
{
public:
    IdEntry(std::string name) :
        name(name),
        orig_id(INVALID_ID),
        new_id(INVALID_ID)
    { }

    std::string toString(void) {
        stringstream ss;
        ss << "{"
            << this->name << ", "
            << this->orig_id << "->"
            << this->new_id
            << "}";
        return ss.str();
    }

    std::string name;
    id_t orig_id;
    id_t new_id;
};

IdDb::IdDb(void)
{
}

IdDb::~IdDb(void)
{
    for (auto entry : this->uids) {
        delete entry;
    }
    for (auto entry : this->gids) {
        delete entry;
    }
}


void IdDb::read(std::string orig_passwd, std::string new_passwd, std::string orig_group, std::string new_group)
{
    this->readFile(orig_passwd, this->uids, "uids", true);
    this->readFile(orig_group, this->gids, "gids", true);
    this->readFile(new_passwd, this->uids, "uids", false);
    this->readFile(new_group, this->gids, "gids", false);

    this->buildIndex(this->uids, this->uidIndex, "uids");
    this->buildIndex(this->gids, this->gidIndex, "gids");
}

void IdDb::readFile(std::string filename, IdEntryList &list, std::string list_name, bool is_orig)
{
    int line_number = 0;
    string line;
    ifstream f;
    f.open(filename);
    while (!f.eof()) {
        getline(f, line);
        ++line_number;
        vector<string> fields = split_string(':', line);
        if (fields.size() >= 2) {
            // Both group and passwd place name and id in same field indecies
            string &name = fields[0];
            string &id_str = fields[2];

            IdEntry &entry = this->getEntry(name, list);
            id_t &id = (is_orig ? entry.orig_id : entry.new_id);
            if (id == INVALID_ID) {
                stringstream(id_str) >> id;
            } else {
                fprintf(stderr, "WARNING: Ignoring duplicate entry for user %s at %s:%d\n",
                    name.c_str(),
                    filename.c_str(),
                    line_number);
            }
        }
    }
}

void IdDb::buildIndex(IdEntryList &list, Int2IdEntryMap &index, std::string list_name)
{
    for (IdEntry *entry : list) {
        auto iter = index.find(entry->orig_id);
        if (iter == index.end()) {
            index[entry->orig_id] = entry;
        } else {
            fprintf(stderr, "WARNING: Ignoring conflict of %s with existing %s in %s\n",
                entry->toString().c_str(),
                iter->second->toString().c_str(),
                list_name.c_str());
        }
    }

    unsigned long entries_before = index.size();
    for (auto iter = index.begin(); iter != index.end();) {
        IdEntry &entry = *iter->second;
        if (entry.orig_id == entry.new_id) {
            // Remove ids that have not changed
            index.erase(iter++);
        } else {
            ++iter;
        }
    }
    fprintf(stderr, "Removed %lu unchanged entries from %s\n",
            entries_before - index.size(),
            list_name.c_str());
}

IdEntry &IdDb::getEntry(std::string name, IdEntryList &list)
{
    auto iter = find_if(list.begin(), list.end(), [&name](IdEntry* e) -> bool { return e->name == name; });
    if (iter == list.end()) {
        IdEntry *entry = new IdEntry(name);
        list.push_back(entry);
        return *entry;
    } else {
        return *(*iter);
    }
}

id_t IdDb::transform(id_t id, Int2IdEntryMap &index)
{
    auto iter = index.find(id);
    if (iter == index.end()) {
        return id;
    } else {
        return iter->second->new_id;
    }
}
