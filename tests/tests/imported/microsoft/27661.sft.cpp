//type:cp
//options_all:--microsoft_v 1940 --ms_c++20
template <auto N>
struct StringLitteral {
    constexpr StringLitteral(const char(&str)[N]) {
        for (size_t i = 0; i < N; i++)
        {
            value[i] = str[i];
        }

    }

    char value[N];
};

template <StringLitteral Name>
class ServiceUsage {
public:

    void setEnable(bool enable) { m_enabled = enable; }
    bool isEnabled() const noexcept { return m_enabled; }
    constexpr auto getName() const { return Name; }

private:

    bool m_enabled{ false };
};

int main()
{
    ServiceUsage<"hello"> usage;
    bool ok = usage.isEnabled();
    return 0;
}
