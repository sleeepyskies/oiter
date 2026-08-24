#pragma once

#include <lyra/lyra.hpp>
#include <memory>
#include <string>

#include "app.hpp"

namespace oiter {
/**
 * @brief Base class for commands.
 */
class Command {
public:
    /** @brief Ensures commands can be destroyed through the base interface. */
    virtual ~Command() = default;

    /** @brief Creates the lyra parser for this command. */
    [[nodiscard]] virtual auto create_parser() -> lyra::command = 0;

    /** @brief Executes the command using its parsed options. */
    virtual auto run() -> void = 0;

    /** @brief Checks whether this command was selected during parsing. */
    [[nodiscard]] auto selected() const noexcept -> bool;

protected:
    /**
     * @brief Constructs the shared state of a CLI command.
     * @param name Name used to select the command.
     * @param description Description displayed in the CLI help.
     */
    Command(std::string name, std::string description);

    /** @brief Creates a parser containing the common command configuration. */
    [[nodiscard]] auto create_base_parser() -> lyra::command;

private:
    std::string m_name;
    std::string m_description;
    bool m_selected = false;
};

/**
 * @brief Runs Oiter in interactive mode.
 */
class InteractiveCommand final : public Command {
public:
    InteractiveCommand();
    [[nodiscard]] auto create_parser() -> lyra::command override;
    auto run() -> void override;

private:
    InteractiveAppOptions m_options;
};

/**
 * @brief Renders a single image.
 */
class RenderCommand final : public Command {
public:
    RenderCommand();
    [[nodiscard]] auto create_parser() -> lyra::command override;
    auto run() -> void override;

private:
    RenderAppOptions m_options;
};

/**
 * @brief Creates and parses oiter's cli.
 */
struct Cli {
    /**
     * @brief Parses the cil arguments.
     * @param argc Number of cli arguments.
     * @param argv Cli argument values.
     * @return The selected command, or nullptr when help was requested.
     * @throws std::runtime_error If the cli args are invalid.
     */
    [[nodiscard]] static auto parse(
        int argc,
        const char** argv
    ) -> std::unique_ptr<Command>;
};
} // namespace oiter
