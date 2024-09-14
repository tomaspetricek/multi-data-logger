#include <array>
#include <cstddef>
#include <format>
#include <print>
#include <tuple>
#include <type_traits>
#include <string_view>
#include <cassert>

namespace tp
{
    using can_id_t = std::int32_t;

    constexpr std::size_t max_data_size = 6;
    using can_data_t = std::array<std::size_t, max_data_size>;

    enum class log_level
    {
        info,
        warning,
        error,
        fatal,
        count
    };

    std::string_view to_string(const log_level &level)
    {
        assert(level < log_level::count);
        static std::array<std::string_view, static_cast<std::size_t>(log_level::count)> names{
            {
                {"info"},
                {"warning"},
                {"error"},
                {"fatal"},
            }};
        return names[static_cast<std::size_t>(level)];
    }

    class can_logger
    {
    public:
        struct message
        {
            can_id_t can_id;
            can_data_t data;
        };
        using message_type = message;

        template <log_level Level>
        void log(const message &msg)
        {
            std::println("{}: can logger: can id: {}, data: {}", to_string(Level), msg.can_id, msg.data);
        }
    };

    class file_logger
    {
    public:
        using format_type = char const *;

        template <format_type Format, class... Args>
        struct message
        {
            using argumnets_type = std::tuple<const Args &...>;

            template <class... Arguments>
            explicit message(const Arguments &...args_) noexcept : args{std::tie(args_...)} {}

            argumnets_type args;
        };

        template <format_type Format, class... Args>
        using message_type = message<Format, Args...>;

        template <log_level Level, format_type Format, class... Args>
        void log(const message<Format, Args...> &msg)
        {
            std::apply(
                [&](auto &...args)
                {
                    std::print("{}: file logger: ", to_string(Level));
                    std::println(Format, args...);
                },
                msg.args);
        }
    };

    struct input_data
    {
        std::size_t length, width, height;
    };

    template <can_id_t CanId>
    class message_builder
    {
    public:
        template <class Logger>
        static auto create_message(const input_data &data) noexcept
        {
            if constexpr (std::is_same_v<Logger, can_logger>)
            {
                return can_logger::message{CanId, can_data_t{data.length, data.width, data.height}};
            }
            else
            {
                static_assert(std::is_same_v<Logger, file_logger>);
                static constexpr const char format[] = "input data: length: {}, width: {}, height: {}";
                return file_logger::message<format, std::size_t, std::size_t, std::size_t>{
                    data.length, data.width, data.height};
            }
        }
    };

    template <log_level Level, class Data, class MessageBuilder, class... Loggers>
    void log(const Data &data, const MessageBuilder &builder, std::tuple<Loggers &...> loggers) noexcept
    {
        std::apply(
            [&](Loggers &...loggers_)
            {
                auto log = [&](auto &logger)
                {
                    using logger_t = std::decay_t<decltype(logger)>;
                    const auto message = builder.template create_message<logger_t>(data);
                    logger.template log<Level>(message);
                };
                (log(loggers_), ...);
            },
            loggers);
    }

    template <class... Loggers>
    class component
    {
        static constexpr can_id_t can_id{10};
        using message_builder_type = message_builder<can_id>;
        using loggers_type = std::tuple<Loggers &...>;

        loggers_type loggers_;
        message_builder_type message_builder_;

    public:
        template <class... Logs>
        explicit component(Logs &...loggers) noexcept
            : loggers_{std::tie(loggers...)} {}

        void process(const input_data &input)
        {
            log<log_level::info>(input, message_builder_, loggers_);
        }
    };
} // namespace tp

int main()
{
    tp::can_logger can_logger;
    tp::file_logger file_logger;
    tp::component<tp::can_logger, tp::file_logger> comp{can_logger, file_logger};
    comp.process(tp::input_data{1, 2, 3});
}
