import random
import os

# 生成随机字符串
def genrate_random_string(length: int = 10):
    letters = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"
    return "".join(random.choice(letters) for i in range(length))


# 生成随机文件名
def genrate_random_filename(extension):
    return genrate_random_string(10) + "." + extension


# 生成随机整数
def genrate_random_integer(start: int = -2147483648, end: int = 2147483647):
    return random.randint(start, end)


# 生成随机浮点数
def genrate_random_float(start: float = -1e300, end: float = 1e300):
    return random.uniform(start, end)


# 生成随机布尔值
def genrate_random_boolean():
    return random.choice([True, False])


# 生成随机列表
def genrate_random_list(length: int = 10, item_generator=genrate_random_integer):
    return [item_generator() for _ in range(length)]


# 生成随机字典
def genrate_random_dict(
    length: int = 10,
    key_generator=genrate_random_string,
    value_generator=genrate_random_string,
):
    return {key_generator(): value_generator() for _ in range(length)}


# dump to string
def dump_to_string(data):
    # print(data, type(data))
    if data is None:
        # print("data is None")
        return "{}"
    if isinstance(data, str):
        # print("data is str")
        return '"' + data + '"'
    if isinstance(data, bool):
        # print("data is bool")
        return str(data).lower()
    if isinstance(data, (int, float)):
        # print("data is number")
        return str(data)
    if isinstance(data, (list, tuple)):
        # print("data is list")
        return "{" + ",".join(dump_to_string(item) for item in data) + "}"
    if isinstance(data, dict):
        # print("data is map")
        return (
            "{"
            + ",".join(
                "{" + "{}, {}".format(dump_to_string(k), dump_to_string(v)) + "}"
                for k, v in data.items()
            )
            + "}"
        )
    return "{}"


# 生成随机结构体
def genrate_random_structure(
    field_num: int,
    field_avaliable_types: dict,
    struct_name: str = "",
    base_class: str = "",
    head_fill: str = "",
    tail_fill: str = "",
    **args
):
    # 生成结构体名
    if struct_name == "":
        struct_name = "TestStruct_" + genrate_random_string(10)
    if base_class != "":
        struct_name = base_class + " " + struct_name + " : " + base_class + "{\n"
    else:
        ret = "struct " + struct_name + " {\n"
    # 生成字段名
    field_names = []
    if args.get("field_names") is not None:
        field_names = args["field_names"]
        if len(field_names) > field_num:
            field_names = field_names[:field_num]
        elif len(field_names) < field_num:
            field_names += [
                "field_" + str(i) + "_" + genrate_random_string(5)
                for i in range(len(field_names), field_num)
            ]
    else:
        for i in range(field_num):
            field_names.append("field_" + str(i) + "_" + genrate_random_string(5))
    # 生成字段类型
    field_types = []
    if args.get("field_types") is not None:
        field_types = args["field_types"]
        if len(field_types) > field_num:
            field_types = field_types[:field_num]
        elif len(field_types) < field_num:
            field_types += [
                random.choice(list(field_avaliable_types.keys()))
                for i in range(len(field_types), field_num)
            ]
    else:
        field_types = [
            random.choice(list(field_avaliable_types.keys())) for i in range(field_num)
        ]

    def replace(x: str):
        x = x.replace("$struct_name", struct_name)
        x = x.replace("$field_num", str(field_num))
        x = x.replace("$field_names", ",".join(field_names))
        x = x.replace("$field_types", ",".join(field_types))
        x = x.replace("$base_class", base_class)
        for i in range(field_num):
            x = x.replace("$field_" + str(i), field_names[i])
            x = x.replace("$field_type_" + str(i), field_types[i])
        return x

    # 填充结构体头
    if head_fill != "":
        head_fill = replace(head_fill)
        ret += head_fill + "\n"

    # 生成字段定义
    for i in range(field_num):
        ret += "    " + field_types[i] + " " + field_names[i]
        if field_avaliable_types.get(field_types[i]) is not None:
            ret += (
                " = " + dump_to_string(field_avaliable_types[field_types[i]]()) + ";\n"
            )
        else:
            ret += " = {};\n"

    # 填充结构体尾
    if tail_fill != "":
        tail_fill = replace(tail_fill)
        ret += tail_fill + "\n"
    ret += "};\n"
    return ret


def genrate_random_enum(enum_count: int, enum_name: str = "", enum_values: list = []):
    if enum_count > len(enum_values):
        enum_values += [
            "Enum" + str(i) + "_" + genrate_random_string(5)
            for i in range(len(enum_values), enum_count)
        ]
    if enum_name == "":
        enum_name = "TestEnum" + genrate_random_string(5)
    return "enum " + enum_name + " {\n" + ",\n".join(enum_values) + "\n};\n"


# 常用类型
field_avaliable_types = {
    "int": genrate_random_integer,
    "double": genrate_random_float,
    "bool": genrate_random_boolean,
    "std::string": genrate_random_string,
    "std::vector<int>": lambda len=10: genrate_random_list(len, genrate_random_integer),
    "std::map<std::string, int>": lambda len=10: genrate_random_dict(
        len, genrate_random_string, genrate_random_integer
    ),
}


if __name__ == "__main__":
    print(genrate_random_enum(62))
    # 生成随机struct
    # for i in range(1, 21):
    #     struct_name = "TestStruct_" + str(i)
    #     print(
    #         genrate_random_structure(
    #             i,
    #             field_avaliable_types,
    #             tail_fill=(
    #                 """
    # NEKO_SERIALIZER($field_names)
    # NEKO_DECLARE_PROTOCOL($struct_name, JsonSerializer)
    # """
    #             ),
    #             struct_name=struct_name,
    #         )
    #     )
