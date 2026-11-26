//options_all:--microsoft_v 1928
struct wrapper { char data[4]; };

struct pr_block {
    constexpr pr_block() = default;
    constexpr pr_block(pr_block &&Array) {}
    constexpr pr_block(const pr_block &Array) : Elem{ Array.Elem } {}

    wrapper Elem{};
};

constexpr pr_block blk_impl() {
    pr_block blk{};
    return true ? blk : pr_block{};
}

void blk() { blk_impl(); }
