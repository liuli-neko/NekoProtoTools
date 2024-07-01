import random
import os

# 生成随机字符串
def generate_random_string(length):
    letters = 'abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789'
    return ''.join(random.choice(letters) for i in range(length))

# 生成随机文件名
def generate_random_filename(extension):
    return generate_random_string(10) + '.' + extension

# 生成随机整数
def generate_random_integer(start, end):
    return random.randint(start, end)

# 生成随机浮点数
def generate_random_float(start, end):
    return random.uniform(start, end)

# 生成随机布尔值
def generate_random_boolean():
    return random.choice([True, False])

# 生成随机列表
def generate_random_list(length, item_generator):
    return [item_generator() for _ in range(length)]

# 生成随机字典
def generate_random_dict(length, key_generator, value_generator):
    return {key_generator(): value_generator() for _ in range(length)}

# dump to string
def dump_to_string(data):
    if data is None:
        return "{}"
    if isinstance(data, str):
        return '"' + data + '"'
    if isinstance(data, (int, float)):
        return str(data)
    if isinstance(data, bool):
        return str(data).lower()
    if isinstance(data, (list, tuple)):
        return '[' + ','.join(dump_to_string(item) for item in data) + ']'
    if isinstance(data, dict):
        return '{' + ','.join('{' + '{}, {}'.format(k, v) + '}' for k, v in data.items()) + '}'
    return "{}"

# 生成随机结构体
def generate_random_structure(field_num, field_types, struct_name = "", base_class = "", head_fill = "", tail_fill = "", **args):
    if struct_name is "":
        struct_name = "TestStruct_" + generate_random_string(10)
    if base_class != "":
        struct_name = base_class + " " + struct_name + " : " + base_class + "{\n";
    else :
        ret = "struct " + struct_name + " {\n"
    field_names = []
    for i in range(field_num):
        field_names.append("field_" + str(i) + "_" + generate_random_string(5))

    if head_fill != "":
        head_fill = head_fill.replace("$struct_name", struct_name)
        for i in range(field_num):
            head_fill = head_fill.replace("$field_" + str(i), field_names[i])
        ret += head_fill + "\n";

    for i in range(field_num):
        field_type = random.choice(field_types)
        field_name = field_names[i]
        ret += "    " + field_type + " " + field_name + " = {};\n"

    if tail_fill != "":
        tail_fill = tail_fill.replace("$struct_name", struct_name)
        for i in range(field_num):
            tail_fill = tail_fill.replace("$field_" + str(i), field_names[i])
        ret += tail_fill + "\n";
    ret += "};\n"
    return ret

if __name__ == "__main__":
    # 生成随机struct
    print(generate_random_structure(10, ["int", "double", "bool", "std::string", "std::vector<int>", "std::map<int, std::string>"], tail_fill="    NEKO_SERIALIZE($struct_name)"))