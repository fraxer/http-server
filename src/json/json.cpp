#include "json.h"

using namespace std;

namespace server {

Json::Json() {}

Json::Json(const Json& var) {
    
    if(var.tag == STRING) {
        
        tag = STRING;

        if(this->p != nullptr) {
            this->deletePointer();
        }

        this->p = new string(*(static_cast<string*>(var.p)));

        return;
    }
    else if(var.tag == ARRAY) {

        tag = ARRAY;

        if(this->p != nullptr) {
            this->deletePointer();
        }

        this->p = new vector< unordered_map<string, Json*>* >(*(static_cast<vector< unordered_map<string, Json*>* >*>(var.p)));

        return;
    }
    else if(var.tag == OBJECT) {

        tag = OBJECT;

        if(this->p != nullptr) {
            this->deletePointer();
        }

        this->p = new unordered_map<string, Json*>(*(static_cast< unordered_map<string, Json*>* >(var.p)));

        return;
    }
}

Json::Json(string&& var) {
    // cout << "string" << endl;

    tag = STRING;

    if(this->p != nullptr)
        this->deletePointer();
    
    this->p = new string(var);
}

Json::Json(const string& var) {
    // cout << "string" << endl;

    tag = STRING;

    if(this->p != nullptr)
        this->deletePointer();
    
    this->p = new string(var);
}

Json::Json(vector< unordered_map<string, Json*>* > var) {
    // cout << "vector" << endl;

    tag = ARRAY;

    if(this->p != nullptr)
        this->deletePointer();

    this->p = new vector< unordered_map<string, Json*>* >(var);
}

Json::Json(vector< unordered_map<string, Json*>* >* var) {
    // cout << "vector" << endl;

    tag = ARRAY;

    if(this->p != nullptr)
        this->deletePointer();

    this->p = var;
}

Json::Json(unordered_map<string, Json*> var) {
    // cout << "vector" << endl;

    tag = OBJECT;

    if(this->p != nullptr)
        this->deletePointer();

    this->p = new unordered_map<string, Json*>(var);
}

Json::Json(unordered_map<string, Json*>* var) {

    tag = OBJECT;

    if(this->p != nullptr)
        this->deletePointer();

    this->p = var;
}

Json& Json::operator=(const Json& var) {

    if(var.tag == STRING) {
        tag = STRING;

        if(this->p != nullptr)
            this->deletePointer();

        this->p = new string(*(static_cast<string*>(var.p)));

        return *this;
    }
    else if(var.tag == ARRAY) {
        tag = ARRAY;

        if(this->p != nullptr)
            this->deletePointer();

        this->p = new vector< unordered_map<string, Json*>* >(*(static_cast<vector< unordered_map<string, Json*>* >*>(var.p)));

        return *this;
    }
    else if(var.tag == OBJECT) {
        tag = OBJECT;

        if(this->p != nullptr)
            this->deletePointer();

        this->p = new unordered_map<string, Json*>(*(static_cast<unordered_map<string, Json*>*>(var.p)));

        return *this;
    }

    return *this; // return the result by reference
}

string& Json::getString() {
    return *(static_cast<string*>(p));
}

vector< unordered_map<string, Json*>* >& Json::getArray() {
    return *(static_cast<vector< unordered_map<string, Json*>* >*>(this->p));
}

unordered_map<string, Json*>& Json::getObject() {
    return *(static_cast<unordered_map<string, Json*>*>(this->p));
}

JsonTypes Json::type() {
    return this->tag;
}

void Json::deletePointer() {

    // printf("destruct ");

    if(tag == STRING) {
        // printf("string: %s\n", (static_cast<string*>(p))->c_str());
        if(this->p != nullptr) {
            delete static_cast<string*>(this->p);
        }
    }
    else if(tag == ARRAY) {
        // printf("array\n");
        if(this->p != nullptr) {
            vector< unordered_map<string, Json*>* >* v = static_cast<vector< unordered_map<string, Json*>* >*>(this->p);

            for(unsigned int i = 0; i < v->size(); i++) {

                for (auto& x : *v->at(i)) {
                    delete x.second;
                }
                v->at(i)->clear();
            }

            delete static_cast<vector< unordered_map<string, Json*>* >*>(this->p);
        }
    }
    else if(tag == OBJECT) {
        // printf("object\n");
        if(this->p != nullptr) {

            unordered_map<string, Json*>* m = static_cast<unordered_map<string, Json*>*>(this->p);

            for (auto& x : *m) {
                delete x.second;
            }

            m->clear();

            delete static_cast<unordered_map<string, Json*>*>(this->p);
        }
    }
}

Json::~Json() {
    this->deletePointer();
}

Json* Json::parse(char* input, ssize_t length) {
    if(!length) return nullptr;

    if(*input == '{') {
        unordered_map<string, Json*>* object = Json::deserializeObject(input, length);

        if (object) {
            return new Json(object);
        }
    } else if (*input == '[') {
        vector< unordered_map<string, Json*>* >* array = Json::deserializeArray(input, length);

        if (array) {
            return new Json(array);
        }
    }

    return nullptr;
}

unordered_map<string, Json*>* Json::deserializeObject(char* input, ssize_t length) {

    if(!length) return nullptr;

    unique_ptr<char> p(new char[length + 1]);

    strcpy(p.get(), input);

    char* str = p.get();

    unordered_map<string, Json*>* list = new unordered_map<string, Json*>();

    if(length >= 1 && str[length - 1] == '}') {
        str[length - 1] = 0; // } => \0
    } else {
        printf("error: последний символ не }\n");
        delete list;
        return nullptr; // error
    }

    if(*str == '{') {
        str++; // {
    } else {
        printf("error: первый символ не {\n");
        delete list;
        return nullptr; // error
    }

    char* key        = nullptr;
    char* value      = nullptr;
    bool  in_quotes  = false;
    bool  key_finded = false;
    bool  sep_finded = false;

    // {
    //     "id" asd:: "0",
    //     st"ring:"asdasd\"as12312\"\"",
    //     object_string: "{asd:123}",
    //     object: {
    //         asd: 1asd23,
    //         zxc: "asd"
    //     },
    //     ar: [],
    //     array_string:"[0,1,3,\"asdasda\"]",
    //     array:[0,1,3]
    // }

    while(*str) {
        if(isspace(*str)) {
            if(key && !in_quotes && !value && !sep_finded) {
                key_finded = true;
                *str = 0;
            }
            str++;
            continue;
        }
        if(*str == '"') {
            if (key && !in_quotes && !value && !sep_finded) {
                printf("error: ковычка в середине ключа: %s\n", key);
                for(auto& l : *list) {
                    delete l.second;
                }
                delete list;
                return nullptr;
            }
            if(!in_quotes && !key_finded) {
                key = str + 1;
                in_quotes = true;
            }
            else if (key && in_quotes && !key_finded) {
                key_finded = true;
                in_quotes  = false;
                *str = 0;
            }
            else if (key_finded && sep_finded && !in_quotes) {
                str++;
                value = str;
                in_quotes = true;
                while(*str) {
                    if(*str == '"' && *(str - 1) != '\\') {
                        *str = 0;
                        str++;
                        in_quotes  = false;
                        key_finded = false;
                        sep_finded = false;

                        (*list)[key] = new Json(value);

                        // printf("%s -> %s\n", key, value);

                        key = nullptr;
                        value = nullptr;
                        break;
                    }
                    str++;
                }
                if(*str == ',') {
                    *str = 0;
                } else if(*str != 0) {
                    printf("error: нет запятой после строкового значения\n");
                    for(auto& l : *list) {
                        delete l.second;
                    }
                    delete list;
                    return nullptr; // error
                }
            }
            // else if (!in_quotes && key_finded && sep_finded) {
            // }
        }
        else if (*str == ':' && !in_quotes && key_finded && !value && !sep_finded) {
            sep_finded = true;
        }
        else if (key_finded && !sep_finded && *str != ':' && !value) {
            printf("error: между ключом и : символы: %s\n", key);
            for(auto& l : *list) {
                delete l.second;
            }
            delete list;
            return nullptr;
        }
        else if (key_finded && sep_finded && !value) {
            if (*str == '{') {

                char* object_string = str;
                bool object_finded = false;

                value = str;
                int open_qutes = 1;
                str++;
                while(*str) {
                    if(open_qutes == 0) {
                        if(*str == ',') {
                            *str = 0;
                        } else if(*str != 0 && !isspace(*str)) {
                            printf("error: ошибка разбора объекта. нет запятой в конце\n");
                            for(auto& l : *list) {
                                delete l.second;
                            }
                            delete list;
                            return nullptr;
                        }
                        object_finded = true;
                        break;
                    }
                    if(*str == '{') {
                        open_qutes++;
                    } else if (*str == '}') {
                        open_qutes--;

                        if (open_qutes == 0 && str[1] == 0) {
                            object_finded = true;
                            str++;
                            break;
                        }
                    }
                    str++;
                }
                if (object_finded) {
                    sep_finded = false;
                    key_finded = false;

                    unordered_map<string, Json*>* l = deserializeObject(object_string, str - object_string);

                    if(l) {
                        (*list)[key] = new Json(l);
                    }

                    key = nullptr;
                    value = nullptr;
                }
                if(open_qutes != 0) {
                    printf("error: ошибка разбора объекта\n");
                    for(auto& l : *list) {
                        delete l.second;
                    }
                    delete list;
                    return nullptr;
                }
            } else if (*str == '[') {

                char* array_string = str;
                bool array_finded = false;

                value = str;
                int open_qutes = 1;
                str++;
                while(*str) {
                    if(open_qutes == 0) {
                        if(*str == ',') {
                            *str = 0;
                        } else if(*str != 0 && !isspace(*str)) {
                            printf("error: ошибка разбора массива. нет запятой в конце\n");
                            for(auto& l : *list) {
                                delete l.second;
                            }
                            delete list;
                            return nullptr;
                        }
                        array_finded = true;
                        break;
                    }
                    if(*str == '[') {
                        open_qutes++;
                    } else if (*str == ']') {
                        open_qutes--;

                        if (open_qutes == 0 && str[1] == 0) {
                            array_finded = true;
                            str++;
                            break;
                        }
                    }
                    str++;
                }
                if (array_finded) {
                    sep_finded = false;
                    key_finded = false;

                    vector< unordered_map<string, Json*>* >* l = deserializeArray(array_string, str - array_string);

                    if(l) {
                        (*list)[key] = new Json(l);
                    }

                    key = nullptr;
                    value = nullptr;
                }
                if(open_qutes != 0) {
                    printf("error: ошибка разбора массива\n");
                    for(auto& l : *list) {
                        delete l.second;
                    }
                    delete list;
                    return nullptr;
                }
            }
            else if (*str == '-' || isdigit(*str)) {
                value = str;
                str++;
                bool find_point = false;
                while(isdigit(*str) || *str == '.') {
                    if (*str == '.') {
                        if (find_point) break;
                        find_point = true;
                    }
                    str++;
                }
                if(*str == ',' && *(str - 1) != '-') {
                    *str = 0;
                    (*list)[key] = new Json(value);
                    key = nullptr;
                    value = nullptr;
                    sep_finded = false;
                    key_finded = false;
                    // str++;
                }
                // else if (*str == 0) {
                //     (*list)[key] = new Json(value);
                //     key = nullptr;
                //     value = nullptr;
                //     sep_finded = false;
                //     key_finded = false;
                //     break;
                // }
                else if (*str != 0 && !isspace(*str)) {
                    printf("error: в числовом значении другие сиимволы: %s\n", value);
                    for(auto& l : *list) {
                        delete l.second;
                    }
                    delete list;
                    return nullptr; // error
                }
            }
            else if (tolower(str[0]) == 't' && tolower(str[1]) == 'r' && tolower(str[2]) == 'u' && tolower(str[3]) == 'e') {
                value = str;
                str += 4;
                *str = 0;
                (*list)[key] = new Json(value);
                key = nullptr;
                value = nullptr;
                sep_finded = false;
                key_finded = false;
            }
            else if (tolower(str[0]) == 'f' && tolower(str[1]) == 'a' && tolower(str[2]) == 'l' && tolower(str[3]) == 's' && tolower(str[4]) == 'e') {
                value = str;
                str += 5;
                *str = 0;
                (*list)[key] = new Json(value);
                key = nullptr;
                value = nullptr;
                sep_finded = false;
                key_finded = false;
            }
            else {
                printf("error: строковое значение не взято в кавычки: %s\n", value);
                for(auto& l : *list) {
                    delete l.second;
                }
                delete list;
                return nullptr; // error
            }
        }
        else if (*str == ',' && !in_quotes && !key_finded) {
            str++;
            continue;
        }
        else if (!key) {
            key = str;
        }
        else if (key && *str == ':') {
            key_finded = true;
            sep_finded = true;
            *str = 0;
        }

        str++;
    }

    if(in_quotes) {
        printf("error: незакрытая ковычка\n");
        for(auto& l : *list) {
            delete l.second;
        }
        delete list;
        return nullptr; // error
    }
    if(key_finded && sep_finded && value) {
        (*list)[key] = new Json(value);
    }
    if(key && key_finded && !value) {
        printf("error: последняя пара не обработана\n");
        for(auto& l : *list) {
            delete l.second;
        }
        delete list;
        return nullptr; // error
    }

    return list;
}

vector< unordered_map<string, Json*>* >* Json::deserializeArray(char* input, ssize_t length) {

    if(!length) return nullptr;

    unique_ptr<char> p(new char[length + 1]);

    strcpy(p.get(), input);

    char* str = p.get();

    vector< unordered_map<string, Json*>* >* list = new vector< unordered_map<string, Json*>* >();

    if(length >= 1 && str[length - 1] == ']') {
        str[length - 1] = 0; // } => \0
    } else {
        printf("error: последний символ не ]\n");
        delete list;
        return nullptr; // error
    }

    if(*str == '[') {
        str++; // {
    } else {
        printf("error: первый символ не [\n");
        delete list;
        return nullptr; // error
    }

    char* value        = nullptr;
    bool  in_quotes    = false;
    bool  value_finded = false;
    bool  comma_finded = true;
    int   pos          = 0;

    // [
    //     "asd4d",
    //     1232312,
    //     {
    //         asd: 123
    //     },
    //     [1,2,3]
    // ]

    while(*str) {
        if(isspace(*str)) {
            str++;
            continue;
        }
        if(*str == '"') {
            if(!value && !in_quotes && !value_finded) {
                value = str + 1;
                in_quotes    = true;
                comma_finded = false;
            }
            else if (value && in_quotes && !value_finded) {
                value_finded = true;
                in_quotes    = false;
                *str = 0;
            }
            else if (value && !in_quotes) {
                printf("error: ковычка в нестроковом значении: %s\n", value);
                for (auto& l : *list) {
                    for (auto& sl : *l) {
                        delete sl.second;
                    }
                    l->clear();
                }
                list->clear();
                delete list;
                return nullptr; // error
            }
        }
        else if (*str == ',' && !in_quotes && !comma_finded) {
            comma_finded = true;
            value_finded = false;
            *str = 0;

            unordered_map<string, Json*>* m = new unordered_map<string, Json*>({{to_string(pos), new Json(value)}});

            list->push_back(m);

            value = nullptr;
            pos++;
        }
        else if (*str == ',' && !in_quotes && comma_finded) {
            str++;
            continue;
        }
        else if (*str == '{' && !in_quotes) {

            char* object_string = str;
            bool object_finded = false;

            value = str;
            int open_qutes = 1;
            str++;
            while(*str) {
                if(open_qutes == 0) {
                    if(*str == ',' || isspace(*str)) {
                        *str = 0;
                        comma_finded = true;
                        value_finded = false;
                    } else if(*str != 0 && !isspace(*str)) {
                        printf("error: ошибка разбора объекта. нет запятой в конце\n");
                        for (auto& l : *list) {
                            for (auto& sl : *l) {
                                delete sl.second;
                            }
                            l->clear();
                        }
                        list->clear();
                        delete list;
                        return nullptr;
                    }
                    object_finded = true;
                    break;
                }
                if(*str == '{') {
                    open_qutes++;
                } else if (*str == '}') {
                    open_qutes--;

                    if (open_qutes == 0 && str[1] == 0) {
                        object_finded = true;
                        str++;
                        break;
                    }
                }
                str++;
            }
            if (object_finded) {

                unordered_map<string, Json*>* l = deserializeObject(object_string, str - object_string);

                if(l) {
                    unordered_map<string, Json*>* m = new unordered_map<string, Json*>({{to_string(pos), new Json(l)}});
                    list->push_back(m);
                }

                pos++;
                value = nullptr;
            }
            if(open_qutes != 0) {
                printf("error: ошибка разбора объекта\n");
                for (auto& l : *list) {
                    for (auto& sl : *l) {
                        delete sl.second;
                    }
                    l->clear();
                }
                list->clear();
                delete list;
                return nullptr;
            }
        }
        else if (*str == '[' && !in_quotes) {

            char* array_string = str;
            bool array_finded = false;

            value = str;
            int open_qutes = 1;
            str++;
            while(*str) {
                if(open_qutes == 0) {
                    if(*str == ',' || isspace(*str)) {
                        *str = 0;
                        comma_finded = true;
                        value_finded = false;
                    } else if(*str != 0 && !isspace(*str)) {
                        printf("error: ошибка разбора массива. нет запятой в конце: %s\n", value);
                        for (auto& l : *list) {
                            for (auto& sl : *l) {
                                delete sl.second;
                            }
                            l->clear();
                        }
                        list->clear();
                        delete list;
                        return nullptr;
                    }
                    array_finded = true;
                    break;
                }
                if(*str == '[') {
                    open_qutes++;
                } else if (*str == ']') {
                    open_qutes--;

                    if (open_qutes == 0 && str[1] == 0) {
                        array_finded = true;
                        str++;
                        break;
                    }
                }
                str++;
            }
            if (array_finded) {

                vector< unordered_map<string, Json*>* >* l = deserializeArray(array_string, str - array_string);

                if(l) {
                    unordered_map<string, Json*>* m = new unordered_map<string, Json*>({{to_string(pos), new Json(l)}});
                    list->push_back(m);
                }

                pos++;
                value = nullptr;
            }
            if(open_qutes != 0) {
                printf("error: ошибка разбора массива\n");
                for (auto& l : *list) {
                    for (auto& sl : *l) {
                        delete sl.second;
                    }
                    l->clear();
                }
                list->clear();
                delete list;
                return nullptr;
            }
        }
        else if ((*str == '-' || isdigit(*str)) && !in_quotes && comma_finded) {
            comma_finded = false;
            value = str;
            str++;
            bool find_point = false;
            while(isdigit(*str) || *str == '.') {
                if (*str == '.') {
                    if (find_point) break;
                    find_point = true;
                }
                str++;
            }
            if (*str == ',' && *(str - 1) != '-') {
                continue;
            }
            else if (*str != 0 && !isspace(*str)) {
                printf("error: в числовом значении другие сиимволы: %s\n", value);
                for (auto& l : *list) {
                    for (auto& sl : *l) {
                        delete sl.second;
                    }
                    l->clear();
                }
                list->clear();
                delete list;
                return nullptr; // error
            }
        }
        else if (tolower(str[0]) == 't' && tolower(str[1]) == 'r' && tolower(str[2]) == 'u' && tolower(str[3]) == 'e' && !in_quotes && comma_finded) {
            value = str;
            str += 4;
            continue;
        }
        else if (tolower(str[0]) == 'f' && tolower(str[1]) == 'a' && tolower(str[2]) == 'l' && tolower(str[3]) == 's' && tolower(str[4]) == 'e' && !in_quotes && comma_finded) {
            value = str;
            str += 5;
            continue;
        }
        else if (!value && !in_quotes && comma_finded) {
            value = str;
            comma_finded = false;
        }

        str++;
    }


    if(in_quotes) {
        printf("error: незакрытая ковычка\n");
        for (auto& l : *list) {
            for (auto& sl : *l) {
                delete sl.second;
            }
            l->clear();
        }
        list->clear();
        delete list;
        return nullptr; // error
    }
    if(value && !in_quotes && !comma_finded) {
        unordered_map<string, Json*>* m = new unordered_map<string, Json*>({{to_string(pos), new Json(value)}});
        list->push_back(m);
    }

    return list;
}

} // namespace