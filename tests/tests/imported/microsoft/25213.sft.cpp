//options_all:--c++11
//type:fp
template <class ...attribute_names>
struct special_attribute_pack : virtual attribute_names... {
    template <class T>
    struct on_type : special_attribute_pack<attribute_names>... {};
};
