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
#include "log.h"
#include "il2cpp-tabledefs.h"
#include "il2cpp-class.h"

#define DO_API(r, n, p) r (*n) p

#include "il2cpp-api-functions.h"

#undef DO_API

static void *il2cpp_handle = nullptr;
static uint64_t il2cpp_base = 0;

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

std::string get_method_modifier(uint32_t flags) {
    std::stringstream outPut;
    auto access = flags & METHOD_ATTRIBUTE_MEMBER_ACCESS_MASK;
    switch (access) {
        case METHOD_ATTRIBUTE_PRIVATE:
            outPut << "private ";
            break;
        case METHOD_ATTRIBUTE_PUBLIC:
            outPut << "public ";
            break;
        case METHOD_ATTRIBUTE_FAMILY:
            outPut << "protected ";
            break;
        case METHOD_ATTRIBUTE_ASSEM:
        case METHOD_ATTRIBUTE_FAM_AND_ASSEM:
            outPut << "internal ";
            break;
        case METHOD_ATTRIBUTE_FAM_OR_ASSEM:
            outPut << "protected internal ";
            break;
    }
    if (flags & METHOD_ATTRIBUTE_STATIC) {
        outPut << "static ";
    }
    if (flags & METHOD_ATTRIBUTE_ABSTRACT) {
        outPut << "abstract ";
        if ((flags & METHOD_ATTRIBUTE_VTABLE_LAYOUT_MASK) == METHOD_ATTRIBUTE_REUSE_SLOT) {
            outPut << "override ";
        }
    } else if (flags & METHOD_ATTRIBUTE_FINAL) {
        if ((flags & METHOD_ATTRIBUTE_VTABLE_LAYOUT_MASK) == METHOD_ATTRIBUTE_REUSE_SLOT) {
            outPut << "sealed override ";
        }
    } else if (flags & METHOD_ATTRIBUTE_VIRTUAL) {
        if ((flags & METHOD_ATTRIBUTE_VTABLE_LAYOUT_MASK) == METHOD_ATTRIBUTE_NEW_SLOT) {
            outPut << "virtual ";
        } else {
            outPut << "override ";
        }
    }
    if (flags & METHOD_ATTRIBUTE_PINVOKE_IMPL) {
        outPut << "extern ";
    }
    return outPut.str();
}

bool _il2cpp_type_is_byref(const Il2CppType *type) {
    auto byref = type->byref;
    if (il2cpp_type_is_byref) {
        byref = il2cpp_type_is_byref(type);
    }
    return byref;
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
	
    while (auto method = il2cpp_class_get_methods(klass, &iter)) {
        /*if (method->methodPointer) {
            out << "\tvoid " << il2cpp_method_get_name(method) << "() {";
            out << "\t\t" <<

                outPut << "\t// RVA: 0x";
            outPut << std::hex << (uint64_t)method->methodPointer - il2cpp_base;
            outPut << " VA: 0x";
            outPut << std::hex << (uint64_t)method->methodPointer;
        }
        else {
            outPut << "\t// RVA: 0x VA: 0x0";
        }*/
     
        uint32_t iflags = 0;
        auto flags = il2cpp_method_get_flags(method, &iflags);
        auto return_type = il2cpp_method_get_return_type(method);
        auto return_class = il2cpp_class_from_type(return_type);
        bool isStatic = flags & METHOD_ATTRIBUTE_STATIC;

        out << repeat("\t", namespace_vec.size()) << "\t" << (isStatic ? "static " : "") << il2cpp_class_get_name(return_class) << " " << il2cpp_method_get_name(method) << "(";
        out << (isStatic ? "" : "void* obj, ");

        auto param_count = il2cpp_method_get_param_count(method);
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
		
        out << ") {\n";

		out << repeat("\t", namespace_vec.size()) << "\t\til2cpp_runtime_invoke(getMethod(" << il2cpp_method_get_name(method) << "), " << (isStatic ? "getStaticObject()" : "obj") << ", ";
		for(int i = 0; i < param_count; ++i) {
			auto param = il2cpp_method_get_param(method, i);
			auto attrs = param->attrs;
			auto parameter_class = il2cpp_class_from_type(param);
			out << il2cpp_method_get_param_name(method, i);
			out << ", ";
		}

		if (param_count > 0) {
			out.seekp(-2, out.cur);
		}
		
		out << ");\n";
		
		out << repeat("\t", namespace_vec.size()) << "\t}\n";
    }
}

void dump_property(Il2CppClass* klass, std::ofstream& out) {
    void* iter = nullptr;
    const char* namespace_ = il2cpp_class_get_namespace(klass);
    std::vector<std::string> namespace_vec;
    std::stringstream ss(namespace_);
    std::string item;
    while (std::getline(ss, item, '.')) {
        namespace_vec.push_back(item);
    }

    while (auto prop_const = il2cpp_class_get_properties(klass, &iter)) {
        auto prop = const_cast<PropertyInfo*>(prop_const);
        auto get = il2cpp_property_get_get_method(prop);
        auto set = il2cpp_property_get_set_method(prop);
        auto prop_name = il2cpp_property_get_name(prop);

        Il2CppClass* prop_class = nullptr;
        uint32_t iflags = 0;
        if (get) {
            out << repeat("\t", namespace_vec.size()) << "\t" << il2cpp_class_from_type(il2cpp_method_get_return_type(get)) << "get_" << prop_name << "() {";
            out << repeat("\t", namespace_vec.size()) << "\t\treturn il2cpp_property_get_get_method(getProperty(), \"" << prop_name << "\");";
            out << repeat("\t", namespace_vec.size()) << "\t}";
        }
        else if (set) {
            auto param = il2cpp_method_get_param(set, 0);
            prop_class = il2cpp_class_from_type(param);

            out << repeat("\t", namespace_vec.size()) << "\tvoid set_" << prop_name << "(" << prop_class << " value) {";
            out << repeat("\t", namespace_vec.size()) << "\t\treturn il2cpp_property_get_set_method(getProperty(), \"" << prop_name << "\");";
            out << repeat("\t", namespace_vec.size()) << "\t}";
        }
    }
}

std::string dump_field(Il2CppClass *klass, std::ofstream& out) {
    const char* namespace_ = il2cpp_class_get_namespace(klass);
    auto is_enum = il2cpp_class_is_enum(klass);
    void *iter = nullptr;

    std::vector<std::string> namespace_vec;
    std::stringstream ss(namespace_);
    std::string item;
    while (std::getline(ss, item, '.')) {
        namespace_vec.push_back(item);
    }

    while (auto field = il2cpp_class_get_fields(klass, &iter)) {
        auto attrs = il2cpp_field_get_flags(field);
        auto field_type = il2cpp_field_get_type(field);
        auto field_class = il2cpp_class_from_type(field_type);

        bool isStatic = attrs & FIELD_ATTRIBUTE_STATIC;
        out << repeat("\t", namespace_vec.size()) << "\tstatic void* " << il2cpp_field_get_name(field) << "(" << (isStatic ? "Il2CppObject* obj" : "") << ") {" << "\n";
        out << repeat("\t", namespace_vec.size()) << "\t\tvoid* res;" << "\n";
        out << repeat("\t", namespace_vec.size()) << "\t\til2cpp_field_get_value(" << (isStatic ? "getStaticObject()" : "obj") << ", getField(\"" << il2cpp_field_get_name(field) << "\", res));" << "\n";
		out << repeat("\t", namespace_vec.size()) << "\t\treturn res;" << "\n";
        out << repeat("\t", namespace_vec.size()) << "\t}" << "\n";
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

    auto outPath = std::string(outDir).append("/").append(namespace_).append("/").append(il2cpp_class_get_name(klass)).append(".h");
    std::ofstream out(outPath);

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

    out << il2cpp_class_get_name(klass) << " {" << "\n"; //TODO genericContainerIndex->
    out << repeat("\t", namespace_vec.size()) << "public:" << "\n";

    out << repeat("\t", namespace_vec.size()) << "\tstatic IL2CppClass* getClass() { " << "\n";
    out << repeat("\t", namespace_vec.size()) << "\t\treturn il2cpp_class_from_name(\"Assembly-CSharp.dll\", \"" << namespace_ << "\", \"" << il2cpp_class_get_name(klass) << "\");" << "\n";
    out << repeat("\t", namespace_vec.size()) << "\t} " << "\n";

    out << repeat("\t", namespace_vec.size()) << "\tstatic IL2CppType* getType() { " << "\n";
    out << repeat("\t", namespace_vec.size()) << "\t\treturn il2cpp_class_get_type(getClass());" << "\n";
    out << repeat("\t", namespace_vec.size()) << "\t} " << "\n";

    out << repeat("\t", namespace_vec.size()) << "\tstatic Il2CppObject* getStaticObject() { " << "\n";
    out << repeat("\t", namespace_vec.size()) << "\t\treturn il2cpp_type_get_object(getType());" << "\n";
    out << repeat("\t", namespace_vec.size()) << "\t} " << "\n";

    out << repeat("\t", namespace_vec.size()) << "\tstatic FieldInfo* getField(const char* name) { " << "\n";
    out << repeat("\t", namespace_vec.size()) << "\t\treturn il2cpp_class_get_field_from_name(getClass(), name);" << "\n";
    out << repeat("\t", namespace_vec.size()) << "\t} " << "\n";

    out << repeat("\t", namespace_vec.size()) << "\tstatic PropertyInfo* getProperty(const char* name) { " << "\n";
    out << repeat("\t", namespace_vec.size()) << "\t\treturn il2cpp_class_get_property_from_name(getClass(), name);" << "\n";
    out << repeat("\t", namespace_vec.size()) << "\t} ";

    out << repeat("\t", namespace_vec.size()) << "\tstatic MethodInfo* getMethod(const char* name) { " << "\n";
    out << repeat("\t", namespace_vec.size()) << "\t\treturn il2cpp_class_get_method_from_name(getClass(), name);" << "\n";
    out << repeat("\t", namespace_vec.size()) << "\t} " << "\n";

    out << "\n{";

    dump_field(klass, out);
    dump_property(klass, out);
    dump_method(klass, out);

    for(int i = 0; i < namespace_vec.size(); i++) {
		out << repeat("\t", namespace_vec.size() - i - 1);
		out << "}";
	}

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
                dump_type(type, std::string(outDir).append("/files/sdk/").c_str());
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
                auto outPut = imageStr.str() + dump_type(type);
                outPuts.push_back(outPut);
                dump_type(type, std::string(outDir).append("/files/sdk/").c_str());
            }
        }
    }

    LOGI("dump done!");
}