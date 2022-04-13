//
// Created by Perfare on 2020/7/4.
//

#include "il2cpp_dump.h"
#include <dlfcn.h>
#include <cstdlib>
#include <cstring>
#include <cinttypes>
#include <string>
#include <vector>
#include <sstream>
#include <fstream>
#include <sys/stat.h>
#include "log.h"
#include "il2cpp-tabledefs.h"
#include "il2cpp-class.h"

#define DO_API(r, n, p) r (*n) p

#include "il2cpp-api-functions.h"

#undef DO_API

static void *il2cpp_handle = nullptr;
static uint64_t il2cpp_base = 0;

typedef struct stat Stat;

template <typename T>
std::string join(const T& v, const std::string& delim) {
    std::ostringstream s;
    for (const auto& i : v) {
        if (&i != &v[0]) {
            s << delim;
        }
        s << i;
    }
    return s.str();
}

static void do_mkdir(const char* path)
{
    Stat            st;

    if (stat(path, &st) != 0)
    {
        /* Directory does not exist. EEXIST for race condition */
        char tmp[256];
        char* p = NULL;
        size_t len;

        snprintf(tmp, sizeof(tmp), "%s", path);
        len = strlen(tmp);
        if (tmp[len - 1] == '/')
            tmp[len - 1] = 0;
        for (p = tmp + 1; *p; p++)
            if (*p == '/') {
                *p = 0;
                mkdir(tmp, S_IRWXU);
                *p = '/';
            }
        mkdir(tmp, S_IRWXU);
    }
    else if (!S_ISDIR(st.st_mode))
    {
        errno = ENOTDIR;
    }
}


int getIndex(std::vector<std::string> v, std::string k) {
    auto it = std::find(v.begin(), v.end(), k);

    if (it != v.end()) {
        return it - v.begin();
    }
    else {
        return 0;
    }
}

std::string repeat(const char* text, int count) {
    std::stringstream ss;
    for (int i = 0; i < count; i++) {
        ss << text;
    }
    return ss.str();
}

void init_il2cpp_api() {
#define DO_API(r, n, p) n = (r (*) p)dlsym(il2cpp_handle, #n)

#include "il2cpp-api-functions.h"

#undef DO_API
}

uint64_t get_module_base(const char *module_name) {
    uint64_t addr = 0;
    char line[1024];
    uint64_t start = 0;
    uint64_t end = 0;
    char flags[5];
    char path[PATH_MAX];

    FILE *fp = fopen("/proc/self/maps", "r");
    if (fp != nullptr) {
        while (fgets(line, sizeof(line), fp)) {
            strcpy(path, "");
            sscanf(line, "%" PRIx64"-%" PRIx64" %s %*" PRIx64" %*x:%*x %*u %s\n", &start, &end,
                   flags, path);
#if defined(__aarch64__)
            if (strstr(flags, "x") == 0) //TODO
                continue;
#endif
            if (strstr(path, module_name)) {
                addr = start;
                break;
            }
        }
        fclose(fp);
    }
    return addr;
}

std::string parseType(const char* _type) {
    std::string type = std::string(_type);
    if (type == "Void") {
        return "void";
    }
    else if (type == "String") {
        return "Il2CppString*";
    }
    else if (type == "Int32") {
        return "int32_t";
    } 
    else if (type == "Int64") {
        return "int64_t";
    }
    else if (type == "UInt32") {
        return "uint32_t";
    }
    else if (type == "UInt64") {
        return "uint64_t";
    }
    else if (type == "Boolean") {
        return "bool";
    }
    else if (type == "Int32[]") {
        return "int32_t*";
    }
    else if (type == "Int64[]") {
        return "int64_t*";
    }
    else if (type == "UInt32[]") {
        return "uint32_t*";
    }
    else if (type == "UInt64[]") {
        return "uint64_t*";
    }
    else if (type == "String[]") {
        return "Il2CppString**";
    }
    else {
        return "Il2CppObject*";
    }
}

std::string formatName(std::string name) {
	name.erase(std::remove(name.begin(), name.end(), '<'), name.end());
    name.erase(std::remove(name.begin(), name.end(), '.'), name.end());	
	std::replace(name.begin(), name.end(), '>', '_');
	
    return name;
}

void dump_method(Il2CppClass* klass, std::ofstream& out) {
    void* iter = nullptr;
    const char* namespace_ = il2cpp_class_get_namespace(klass);
    std::vector<std::string> namespace_vec;
    std::stringstream ss(namespace_);
    std::string item;
    while (std::getline(ss, item, '.')) {
        namespace_vec.push_back(item);
    }

	int namespace_count = (int) namespace_vec.size();
	
    while (auto method = il2cpp_class_get_methods(klass, &iter)) {     
        uint32_t iflags = 0;
        auto flags = il2cpp_method_get_flags(method, &iflags);
        auto return_type = il2cpp_method_get_return_type(method);
        auto return_class = il2cpp_class_from_type(return_type);
        bool isStatic = flags & METHOD_ATTRIBUTE_STATIC;
        auto param_count = il2cpp_method_get_param_count(method);
        std::string returnType = parseType(il2cpp_class_get_name(return_class));

        out << repeat("\t", namespace_count) << "\t// " << il2cpp_class_get_name(return_class) << " " << il2cpp_method_get_name(method) << "(";
        for (int i = 0; i < param_count; ++i) {
            auto param = il2cpp_method_get_param(method, i);
            auto attrs = param->attrs;

            auto parameter_class = il2cpp_class_from_type(param);
            out << il2cpp_class_get_name(parameter_class) << " " << il2cpp_method_get_param_name(method, i);
            out << ", ";
        }

        if (param_count > 0) {
            out.seekp(-2, out.cur);
        }

        out << ")\n";

        out << repeat("\t", namespace_count) << "\tstatic " << returnType << " " << formatName(il2cpp_method_get_name(method)) << "(";
        out << (isStatic ? "" : (param_count > 0 ? "void* obj, " : "void* obj"));
        
        for (int i = 0; i < param_count; ++i) {
            auto param = il2cpp_method_get_param(method, i);
            auto attrs = param->attrs;
            
            auto parameter_class = il2cpp_class_from_type(param);
            out << parseType(il2cpp_class_get_name(parameter_class)) << " " << il2cpp_method_get_param_name(method, i);
            out << ", ";
        }

        if (param_count > 0) {
            out.seekp(-2, out.cur);
        }
		
        out << ") {\n";

        size_t args_size = 0;
        for (int i = 0; i < param_count; i++) {
            auto param = il2cpp_method_get_param(method, i);
            auto param_class = il2cpp_class_from_type(param);
            args_size += il2cpp_class_instance_size(param_class);
        }

		out << repeat("\t", namespace_count) << "void** args = (void**)malloc(" << args_size << ");\n";
        for (int i = 0; i < param_count; i++) {
            auto param = il2cpp_method_get_param(method, i);
            auto param_class = il2cpp_class_from_type(param);
            
            out << repeat("\t", namespace_count) << "args[" << i << "] = (void*)" << il2cpp_method_get_param_name(method, i) << ";\n";
        }

		out << repeat("\t", namespace_count) << "\t\t" 
            << (returnType != "void" ? ("return " + (returnType != "Il2CppObject*" ? "*((" + returnType + ") *" : "")) : "")
            << (returnType != "Il2CppObject*" ? "il2cpp_object_unbox(" : "") 
            << "il2cpp_runtime_invoke(getMethod(\"" << il2cpp_method_get_name(method) << "\", " << param_count << "), "
            << (isStatic ? "getStaticObject()" : "obj")
            << ", args, nullptr"
            << (returnType != "Il2CppObject*" ? ")" : "")
            << (returnType != "void" ? ")" : "")
		    << ");\n";
		
		out << repeat("\t", namespace_count) << "\t}\n\n";
    }
}

void dump_field(Il2CppClass *klass, std::ofstream& out) {
    const char* namespace_ = il2cpp_class_get_namespace(klass);
    auto is_enum = il2cpp_class_is_enum(klass);
    void *iter = nullptr;

    std::vector<std::string> namespace_vec;
    std::stringstream ss(namespace_);
    std::string item;
    while (std::getline(ss, item, '.')) {
        namespace_vec.push_back(item);
    }

    int namespace_count = (int)namespace_vec.size();

    while (auto field = il2cpp_class_get_fields(klass, &iter)) {
        auto attrs = il2cpp_field_get_flags(field);
        auto field_type = il2cpp_field_get_type(field);
        auto field_class = il2cpp_class_from_type(field_type);
        auto field_class_name = il2cpp_class_get_name(field_class);
        bool isStatic = attrs & FIELD_ATTRIBUTE_STATIC;
		
        std::string returnType = parseType(field_class_name);
		out << repeat("\t", namespace_count) << "\t// " << il2cpp_class_get_name(field_class) << " " << il2cpp_field_get_name(field) << ";\n";
        out << repeat("\t", namespace_count) << "\tstatic " << returnType << " " << formatName(il2cpp_field_get_name(field)) << "(" << (isStatic ? "" : "Il2CppObject* obj") << ") {" << "\n";
        out << repeat("\t", namespace_count) << "\t\treturn " << (returnType != "Il2CppObject*" ? "*((" + returnType + ") *" : "")
            << (returnType != "Il2CppObject*" ? "il2cpp_object_unbox(" : "") 
            << "il2cpp_field_get_value_object(getField(\"" 
            << il2cpp_field_get_name(field) << "\"), " 
            << (isStatic ? "getStaticObject()" : "obj") 
            << (returnType != "Il2CppObject*" ? ")" : "") 
            << ");" << "\n"
            << repeat("\t", namespace_count) << "\t}\n\n";
    }
}

void dump_type(const Il2CppType *type, const char* outDir) {
    auto* klass = il2cpp_class_from_type(type);
    const char* namespace_ = il2cpp_class_get_namespace(klass);
    std::vector<std::string> namespace_vec;
    std::stringstream ss(namespace_);
    std::string item;
    while (std::getline(ss, item, '.')) {
        namespace_vec.push_back(item);
    }

    int namespace_count = (int)namespace_vec.size();

    std::string outPath;

    if (std::string(namespace_).empty()) {
        outPath = std::string(outDir).append("/").append(il2cpp_class_get_name(klass)).append(".h");
        do_mkdir(std::string(outDir).append("/").c_str());
    }
    else {
        std::string str = join(namespace_vec, "/");
        outPath = std::string(outDir).append("/").append(str).append("/").append(il2cpp_class_get_name(klass)).append(".h");
        do_mkdir(std::string(outDir).append("/").append(str).c_str());
    }

	LOGI("Generating %s", outPath.c_str());

    std::ofstream out(outPath);
    if (!out.is_open()) {
		LOGE("Failed to open %s", outPath.c_str());
		return;
    }

    out << "#pragma once" << "\n";
    out << "#include <IL2Cpp/Il2Cpp.h>" << "\n";
    out << "\n";

	for (auto &item : namespace_vec) {
		out << repeat("\t", getIndex(namespace_vec, item)) << "namespace " << item << " {" << "\n";
	}

	out << repeat("\t", namespace_vec.size());

    auto flags = il2cpp_class_get_flags(klass);
    auto is_valuetype = il2cpp_class_is_valuetype(klass);
    auto is_enum = il2cpp_class_is_enum(klass);

    if (is_enum) {
        out << repeat("\t", namespace_vec.size()) << "enum ";
    } else if (is_valuetype) {
        out << repeat("\t", namespace_vec.size()) << "struct ";
    } else {
        out << repeat("\t", namespace_vec.size()) << "class ";
    }

    out << il2cpp_class_get_name(klass) << " {" << "\n";
    out << repeat("\t", namespace_vec.size()) << "public:" << "\n";

    out << repeat("\t", namespace_count) << "\tstatic const Il2CppAssembly* getAssembly() {\n";
	out << repeat("\t", namespace_count) << "\t\treturn il2cpp_domain_assembly_open(il2cpp_domain_get(), \"Assembly-CSharp.dll\");\n";
    out << repeat("\t", namespace_vec.size()) << "\t}\n\n";

    out << repeat("\t", namespace_count) << "\tstatic const Il2CppImage* getImage() {\n";
    out << repeat("\t", namespace_count) << "\t\treturn il2cpp_assembly_get_image(getAssembly());\n";
    out << repeat("\t", namespace_vec.size()) << "\t}\n\n";

    out << repeat("\t", namespace_vec.size()) << "\tstatic Il2CppClass* getClass() { " << "\n";
    out << repeat("\t", namespace_vec.size()) << "\t\treturn il2cpp_class_from_name(getImage(), \"" << namespace_ << "\", \"" << il2cpp_class_get_name(klass) << "\");" << "\n";
    out << repeat("\t", namespace_vec.size()) << "\t} " << "\n\n";

    out << repeat("\t", namespace_vec.size()) << "\tstatic const Il2CppType* getType() { " << "\n";
    out << repeat("\t", namespace_vec.size()) << "\t\treturn il2cpp_class_get_type(getClass());" << "\n";
    out << repeat("\t", namespace_vec.size()) << "\t} " << "\n\n";

    out << repeat("\t", namespace_vec.size()) << "\tstatic Il2CppObject* getStaticObject() { " << "\n";
    out << repeat("\t", namespace_vec.size()) << "\t\treturn il2cpp_type_get_object(getType());" << "\n";
    out << repeat("\t", namespace_vec.size()) << "\t} " << "\n\n";

    out << repeat("\t", namespace_vec.size()) << "\tstatic FieldInfo* getField(const char* name) { " << "\n";
    out << repeat("\t", namespace_vec.size()) << "\t\treturn il2cpp_class_get_field_from_name(getClass(), name);" << "\n";
    out << repeat("\t", namespace_vec.size()) << "\t} " << "\n\n";

    out << repeat("\t", namespace_vec.size()) << "\tstatic const PropertyInfo* getProperty(const char* name) { " << "\n";
    out << repeat("\t", namespace_vec.size()) << "\t\treturn il2cpp_class_get_property_from_name(getClass(), name);" << "\n";
    out << repeat("\t", namespace_vec.size()) << "\t} " << "\n\n";

    out << repeat("\t", namespace_vec.size()) << "\tstatic const MethodInfo* getMethod(const char* name, int argsCount) { " << "\n";
    out << repeat("\t", namespace_vec.size()) << "\t\treturn il2cpp_class_get_method_from_name(getClass(), name, argsCount);" << "\n";
    out << repeat("\t", namespace_vec.size()) << "\t} " << "\n\n";

    dump_field(klass, out);
    dump_method(klass, out);

    for(int i = 0; i < namespace_vec.size(); i++) {
		out << repeat("\t", namespace_vec.size() - i - 1);
		out << "}\n";
	}

    out << "}";

    out.flush();
    out.close();
}

void il2cpp_dump(void *handle, char *outDir) {
    //initialize
    LOGI("il2cpp_handle: %p", handle);
    il2cpp_handle = handle;
    init_il2cpp_api();
    if (il2cpp_domain_get_assemblies) {
        Dl_info dlInfo;
        if (dladdr((void *) il2cpp_domain_get_assemblies, &dlInfo)) {
            il2cpp_base = reinterpret_cast<uint64_t>(dlInfo.dli_fbase);
        } else {
            LOGW("dladdr error, using get_module_base.");
            il2cpp_base = get_module_base("libil2cpp.so");
        }
        LOGI("il2cpp_base: %" PRIx64"", il2cpp_base);
    } else {
        LOGE("Failed to initialize il2cpp api.");
        return;
    }
    auto domain = il2cpp_domain_get();
    il2cpp_thread_attach(domain);

    //start dump
    LOGI("dumping...");
	
    do_mkdir(outDir);

    size_t size;
    auto assemblies = il2cpp_domain_get_assemblies(domain, &size);
    std::vector<std::string> outPuts;

    if (il2cpp_image_get_class) {
        LOGI("Version greater than 2018.3");

        for (int i = 0; i < size; ++i) {
            auto image = il2cpp_assembly_get_image(assemblies[i]);
            auto classCount = il2cpp_image_get_class_count(image);

            for (int j = 0; j < classCount; ++j) {
                auto klass = il2cpp_image_get_class(image, j);
                auto type = il2cpp_class_get_type(const_cast<Il2CppClass *>(klass));
                dump_type(type, std::string(outDir).append("/files/SDK/Includes").c_str());
            }
        }
    }
    else {
        LOGI("Version less than 2018.3");
		
        auto corlib = il2cpp_get_corlib();
        auto assemblyClass = il2cpp_class_from_name(corlib, "System.Reflection", "Assembly");
        auto assemblyLoad = il2cpp_class_get_method_from_name(assemblyClass, "Load", 1);
        auto assemblyGetTypes = il2cpp_class_get_method_from_name(assemblyClass, "GetTypes", 0);
		
        if (assemblyLoad && assemblyLoad->methodPointer) {
            LOGI("Assembly::Load: %p", assemblyLoad->methodPointer);
        }
        else {
            LOGI("miss Assembly::Load");
            return;
        }
		
        if (assemblyGetTypes && assemblyGetTypes->methodPointer) {
            LOGI("Assembly::GetTypes: %p", assemblyGetTypes->methodPointer);
        }
        else {
            LOGI("miss Assembly::GetTypes");
            return;
        }
		
        typedef void* (*Assembly_Load_ftn)(void*, Il2CppString*, void*);
        typedef Il2CppArray* (*Assembly_GetTypes_ftn)(void*, void*);
		
        for (int i = 0; i < size; ++i) {
            auto image = il2cpp_assembly_get_image(assemblies[i]);
            std::stringstream imageStr;
			
            auto image_name = il2cpp_image_get_name(image);
			
            imageStr << "\n// Dll : " << image_name;
			
            auto imageName = std::string(image_name);
            auto pos = imageName.rfind('.');
            auto imageNameNoExt = imageName.substr(0, pos);
            auto assemblyFileName = il2cpp_string_new(imageNameNoExt.c_str());
            auto reflectionAssembly = ((Assembly_Load_ftn)assemblyLoad->methodPointer)(nullptr, assemblyFileName, nullptr);			
            auto reflectionTypes = ((Assembly_GetTypes_ftn)assemblyGetTypes->methodPointer)(reflectionAssembly, nullptr);
            auto items = reflectionTypes->vector;
			
            for (int j = 0; j < reflectionTypes->max_length; ++j) {
                auto klass = il2cpp_class_from_system_type((Il2CppReflectionType*)items[j]);
                auto type = il2cpp_class_get_type(klass);
                dump_type(type, std::string(outDir).append("/files/SDK/Includes").c_str());
            }
        }
    }

    LOGI("dump done!");
}