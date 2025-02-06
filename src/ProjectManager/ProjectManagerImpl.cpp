#include "ProjectManagerImpl.hpp"
#include <reg/src/AnyId.hpp>
#include <tl/expected.hpp>
#include "COOLLAB_FILE_EXTENSION.hpp"
#include "Cool/File/File.h"
#include "Cool/OSC/OSCManager.h"
#include "Dump/coollab_version.hpp"
#include "Serialization/SProject.h"

namespace Lab::internal {

auto ProjectManagerImpl::project_path() const -> std::filesystem::path
{
    return project_path(_file_name);
}

auto ProjectManagerImpl::project_path(std::string_view file_name) const -> std::filesystem::path
{
    return Cool::File::with_extension(_folder_path / file_name, COOLLAB_FILE_EXTENSION);
}

auto ProjectManagerImpl::info_folder_for_the_launcher() const -> std::optional<std::filesystem::path>
{
    return info_folder_for_the_launcher(_project.uuid);
}

auto ProjectManagerImpl::info_folder_for_the_launcher(reg::AnyId const& project_uuid) const -> std::optional<std::filesystem::path>
{
    if (!_path_to_launcher_info_folder)
        return std::nullopt;
    return *_path_to_launcher_info_folder / reg::to_string(project_uuid);
}

auto ProjectManagerImpl::has_registered_project_to_the_launcher() const -> bool
{
    auto const folder = info_folder_for_the_launcher();
    if (!folder.has_value())
        return false;

    return Cool::File::exists(*folder / "path.txt");
}

static auto window_title(std::filesystem::path const& path) -> std::string
{
    return fmt::format(
        "Coollab {} [{}]",
        coollab_version(),
        Cool::File::weakly_canonical(path)
    );
}

void ProjectManagerImpl::set_project_path(std::filesystem::path const& file_path, SetWindowTitle const& set_window_title)
{
    _folder_path                 = Cool::File::without_file_name(file_path);
    _file_name                   = Cool::File::file_name_without_extension(file_path).string();
    Cool::Path::project_folder() = _folder_path;
    set_window_title(window_title(project_path()));
}

void ProjectManagerImpl::set_project_name(std::string_view file_name, SetWindowTitle const& set_window_title)
{
    _file_name = file_name;
    set_window_title(window_title(project_path()));
}

void ProjectManagerImpl::set_project_path_for_launcher(std::filesystem::path const& file_path) const
{
    auto const folder = info_folder_for_the_launcher();
    if (!folder.has_value())
        return;

    Cool::File::set_content(
        *folder / "path.txt",
        file_path.string()
    );
}

auto ProjectManagerImpl::load(std::filesystem::path const& file_path) -> tl::expected<Project, std::string>
{
    auto       project = Project{};
    auto const error   = do_load(project, file_path);
    if (error)
        return tl::make_unexpected(*error.error_message());
    return project;
}

void ProjectManagerImpl::set_project(Project project, OnProjectLoaded const& on_project_loaded)
{
    _project = std::move(project);
    on_project_loaded();
}

auto ProjectManagerImpl::save(std::filesystem::path const& file_path) -> bool
{
    if (!Cool::File::create_folders_for_file_if_they_dont_exist(file_path))
        return false;
    return do_save(_project, file_path);
}

void ProjectManagerImpl::register_last_write_time(std::filesystem::path const& file_path)
{
    if (!Cool::File::exists(file_path))
        return; // The file doesn't exist, no need to register anything
    _last_write_times[Cool::File::weakly_canonical(file_path)] = Cool::File::last_write_time(file_path);
}

auto ProjectManagerImpl::file_contains_data_that_we_did_not_write_ourselves(std::filesystem::path const& file_path) const -> bool
{
    if (!Cool::File::exists(file_path))
        return false;

    auto const our_last_write_time = _last_write_times.find(Cool::File::weakly_canonical(file_path));
    if (our_last_write_time == _last_write_times.end())
        return true;

    return our_last_write_time->second != Cool::File::last_write_time(file_path);
}

} // namespace Lab::internal