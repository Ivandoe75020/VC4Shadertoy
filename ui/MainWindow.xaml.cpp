#include "pch.h"
#include "MainWindow.xaml.h"
#if __has_include("MainWindow.g.cpp")
#include "MainWindow.g.cpp"
#endif
#include "AppInfo.h"
#include "FFmpegAudioSource.h"
#include "FFmpegEncoder.h"
#include "ExportPipeline.h"
#include "OpenGLShaderRenderer.h"
#include "ShadertoyImporter.h"

#include <robuffer.h>
#include <shellapi.h>
#include <shlobj.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string>
#include <utility>

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;

namespace
{
    constexpr int PreviewWidth = 320;
    constexpr int PreviewHeight = 180;
    constexpr double WindowClientWidth = 800.0;
    constexpr double WindowClientHeight = 450.0;

    winrt::hstring toHString(std::string const& utf8)
    {
        if (utf8.empty())
        {
            return winrt::hstring{};
        }
        const int length = ::MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
        std::wstring wide(static_cast<std::size_t>(length), L'\0');
        ::MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), wide.data(), length);
        return winrt::hstring{ wide };
    }

    std::string passSummary(ShadertoyProject const& project)
    {
        std::string text;
        for (ShaderPass const& pass : project.passes)
        {
            if (!text.empty())
            {
                text += ", ";
            }
            text += pass.name;
        }
        return text;
    }
}

namespace winrt::VideoConverterForShadertoy::implementation
{
    MainWindow::MainWindow()
    {
        InitializeComponent();
        Title(winrt::hstring{ AppInfo::WindowTitle });
        configureWindow();
        AppTitleText().Text(winrt::hstring{ AppInfo::Name });
        previewBitmap_ = winrt::Microsoft::UI::Xaml::Media::Imaging::WriteableBitmap{
            PreviewWidth, PreviewHeight };
        PreviewImage().Source(previewBitmap_);
        const auto dispatcher = winrt::Microsoft::UI::Dispatching::DispatcherQueue::GetForCurrentThread();
        uiTimer_ = dispatcher.CreateTimer();
        uiTimer_.Interval(std::chrono::milliseconds(100));
        uiTimer_.Tick([weak = get_weak()](
            winrt::Microsoft::UI::Dispatching::DispatcherQueueTimer const& sender,
            winrt::Windows::Foundation::IInspectable const& args) {
                if (auto self = weak.get())
                {
                    self->UiTimer_Tick(sender, args);
                }
            });
        uiTimer_.Start();
        updateDurationDisplay();
        showNoProject();
    }

    void MainWindow::configureWindow()
    {
        ExtendsContentIntoTitleBar(true);
        SetTitleBar(AppTitleBar());
        SystemBackdrop(winrt::Microsoft::UI::Xaml::Media::MicaBackdrop{});

        HWND windowHandle = nullptr;
        if (auto windowNative = this->try_as<::IWindowNative>())
        {
            winrt::check_hresult(windowNative->get_WindowHandle(&windowHandle));
        }
        const UINT dpi = windowHandle ? ::GetDpiForWindow(windowHandle) : 96;
        const double scale = static_cast<double>(dpi) / 96.0;

        auto appWindow = AppWindow();
        appWindow.ResizeClient(winrt::Windows::Graphics::SizeInt32{
            static_cast<int32_t>(std::lround(WindowClientWidth * scale)),
            static_cast<int32_t>(std::lround(WindowClientHeight * scale)) });

        if (auto presenter = appWindow.Presenter().try_as<winrt::Microsoft::UI::Windowing::OverlappedPresenter>())
        {
            presenter.IsResizable(false);
            presenter.IsMaximizable(false);
        }

        const auto area = winrt::Microsoft::UI::Windowing::DisplayArea::GetFromWindowId(
            appWindow.Id(), winrt::Microsoft::UI::Windowing::DisplayAreaFallback::Nearest);
        const auto work = area.WorkArea();
        const auto size = appWindow.Size();
        appWindow.Move(winrt::Windows::Graphics::PointInt32{
            work.X + (work.Width - size.Width) / 2,
            work.Y + (work.Height - size.Height) / 2 });
    }

    MainWindow::~MainWindow()
    {
        if (uiTimer_)
        {
            uiTimer_.Stop();
        }
        stopPreview();
    }

    winrt::fire_and_forget MainWindow::LoadJsonButton_Click(
        winrt::Windows::Foundation::IInspectable const&,
        RoutedEventArgs const&)
    {
        auto strong = get_strong();

        winrt::Windows::Storage::Pickers::FileOpenPicker picker;

        HWND windowHandle = nullptr;
        auto windowNative = this->try_as<::IWindowNative>();
        if (!windowNative)
        {
            showError("The file picker cannot be opened.");
            co_return;
        }
        winrt::check_hresult(windowNative->get_WindowHandle(&windowHandle));
        picker.as<::IInitializeWithWindow>()->Initialize(windowHandle);

        picker.SuggestedStartLocation(winrt::Windows::Storage::Pickers::PickerLocationId::Downloads);
        picker.FileTypeFilter().Append(L".json");

        auto file = co_await picker.PickSingleFileAsync();
        if (!file)
        {
            co_return;
        }

        ShadertoyProject loaded;
        std::string error;
        if (!importShadertoyJson(std::filesystem::path{ std::wstring{ file.Path() } }, loaded, error))
        {
            showError(error);
            co_return;
        }

        project_ = std::move(loaded);
        projectLoaded_ = true;
        showLoadedProject();
        showWarnings(project_.warnings);
        OpenVideoButton().Visibility(winrt::Microsoft::UI::Xaml::Visibility::Collapsed);
        OpenFolderButton().Visibility(winrt::Microsoft::UI::Xaml::Visibility::Collapsed);
        startPreview();
        updateControlAvailability();
    }

    winrt::fire_and_forget MainWindow::LoadMp3Button_Click(
        winrt::Windows::Foundation::IInspectable const&,
        RoutedEventArgs const&)
    {
        auto strong = get_strong();
        winrt::Windows::Storage::Pickers::FileOpenPicker picker;

        HWND windowHandle = nullptr;
        auto windowNative = this->try_as<::IWindowNative>();
        if (!windowNative)
        {
            showError("The file picker cannot be opened.");
            co_return;
        }
        winrt::check_hresult(windowNative->get_WindowHandle(&windowHandle));
        picker.as<::IInitializeWithWindow>()->Initialize(windowHandle);
        picker.SuggestedStartLocation(winrt::Windows::Storage::Pickers::PickerLocationId::MusicLibrary);
        picker.FileTypeFilter().Append(L".mp3");

        auto file = co_await picker.PickSingleFileAsync();
        if (!file)
        {
            co_return;
        }

        const winrt::apartment_context uiApartment;
        const std::filesystem::path path{ std::wstring{ file.Path() } };
        const std::wstring fileName{ file.Name() };
        co_await winrt::resume_background();

        FFmpegAudioSource audio;
        const bool opened = audio.open(path);
        const std::string error = opened ? std::string{} : audio.lastError();
        const double duration = opened ? audio.durationSeconds() : 0.0;

        co_await uiApartment;
        if (!opened)
        {
            showError("The MP3 could not be loaded: " + error);
            co_return;
        }
        if (setAudioTrack(path, fileName, duration))
        {
            StatusInfoBar().IsOpen(false);
        }
    }

    void MainWindow::RemoveMp3Button_Click(
        winrt::Windows::Foundation::IInspectable const&,
        RoutedEventArgs const&)
    {
        clearAudioTrack();
    }

    winrt::fire_and_forget MainWindow::ExportButton_Click(
        winrt::Windows::Foundation::IInspectable const&,
        RoutedEventArgs const&)
    {
        auto strong = get_strong();
        if (!projectLoaded_ || exporting_)
        {
            co_return;
        }

        winrt::Windows::Storage::Pickers::FileSavePicker picker;
        HWND windowHandle = nullptr;
        auto windowNative = this->try_as<::IWindowNative>();
        if (!windowNative)
        {
            showError("Cannot export video", "The file picker cannot be opened.");
            co_return;
        }
        winrt::check_hresult(windowNative->get_WindowHandle(&windowHandle));
        picker.as<::IInitializeWithWindow>()->Initialize(windowHandle);
        picker.SuggestedStartLocation(winrt::Windows::Storage::Pickers::PickerLocationId::VideosLibrary);
        auto mp4Extensions = winrt::single_threaded_vector<winrt::hstring>();
        mp4Extensions.Append(L".mp4");
        picker.FileTypeChoices().Insert(L"MP4 video", mp4Extensions);

        std::wstring suggestedName{ L"shader-video.mp4" };
        if (!project_.name.empty())
        {
            suggestedName.clear();
            for (unsigned char character : project_.name)
            {
                if ((character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z') ||
                    (character >= '0' && character <= '9') || character == '-' || character == '_')
                {
                    suggestedName.push_back(static_cast<wchar_t>(character));
                }
                else if (character == ' ' && !suggestedName.empty() && suggestedName.back() != L'-')
                {
                    suggestedName.push_back(L'-');
                }
            }
            while (!suggestedName.empty() && suggestedName.back() == L'-')
            {
                suggestedName.pop_back();
            }
            if (suggestedName.empty())
            {
                suggestedName = L"shader-video";
            }
            suggestedName += L".mp4";
        }
        picker.SuggestedFileName(suggestedName);

        auto file = co_await picker.PickSaveFileAsync();
        if (!file)
        {
            co_return;
        }
        startExport(std::filesystem::path{ std::wstring{ file.Path() } });
    }

    void MainWindow::CancelExportButton_Click(
        winrt::Windows::Foundation::IInspectable const&,
        RoutedEventArgs const&)
    {
        if (exportTask_)
        {
            exportTask_->cancel();
            CancelExportButton().IsEnabled(false);
            ExportProgressText().Text(L"Cancelling export...");
        }
    }

    void MainWindow::OpenVideoButton_Click(
        winrt::Windows::Foundation::IInspectable const&,
        RoutedEventArgs const&)
    {
        if (lastOutputPath_.empty())
        {
            showError("Cannot open video", "No exported video is available.");
            return;
        }
        const auto result = ::ShellExecuteW(nullptr, L"open", lastOutputPath_.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        if (reinterpret_cast<INT_PTR>(result) <= 32)
        {
            showError("Cannot open video", "The exported video could not be opened.");
        }
    }

    void MainWindow::OpenFolderButton_Click(
        winrt::Windows::Foundation::IInspectable const&,
        RoutedEventArgs const&)
    {
        if (lastOutputPath_.empty())
        {
            showError("Cannot open folder", "No exported video is available.");
            return;
        }

        PIDLIST_ABSOLUTE itemIdList = nullptr;
        const HRESULT result = ::SHParseDisplayName(lastOutputPath_.c_str(), nullptr, &itemIdList, 0, nullptr);
        if (FAILED(result))
        {
            showError("Cannot open folder", "The exported video location could not be resolved.");
            return;
        }
        const HRESULT openResult = ::SHOpenFolderAndSelectItems(itemIdList, 0, nullptr, 0);
        ::CoTaskMemFree(itemIdList);
        if (FAILED(openResult))
        {
            showError("Cannot open folder", "File Explorer could not select the exported video.");
        }
    }

    void MainWindow::showNoProject()
    {
        projectLoaded_ = false;
        ShaderNameText().Text(L"No shader loaded");
        ShaderDetailsText().Text(L"");
        StatusInfoBar().IsOpen(false);
        updateControlAvailability();
    }

    void MainWindow::showLoadedProject()
    {
        ShaderNameText().Text(toHString(project_.name));
        ShaderDetailsText().Text(toHString("By " + project_.author + " - Passes: " + passSummary(project_)));
        StatusInfoBar().IsOpen(false);
        updateControlAvailability();
    }

    void MainWindow::showError(std::string const& message)
    {
        StatusInfoBar().Severity(InfoBarSeverity::Error);
        StatusInfoBar().Title(L"Cannot load this shader");
        StatusInfoBar().Message(toHString(message));
        StatusInfoBar().IsOpen(true);
    }

    void MainWindow::showError(std::string const& title, std::string const& message)
    {
        StatusInfoBar().Severity(InfoBarSeverity::Error);
        StatusInfoBar().Title(toHString(title));
        StatusInfoBar().Message(toHString(message));
        StatusInfoBar().IsOpen(true);
    }

    void MainWindow::showWarnings(std::vector<std::string> const& warnings)
    {
        if (warnings.empty())
        {
            return;
        }

        std::string text;
        for (std::string const& warning : warnings)
        {
            if (!text.empty())
            {
                text += "\n";
            }
            text += warning;
        }

        StatusInfoBar().Severity(InfoBarSeverity::Warning);
        StatusInfoBar().Title(L"Shader loaded with warnings");
        StatusInfoBar().Message(toHString(text));
        StatusInfoBar().IsOpen(true);
    }

    void MainWindow::DurationComboBox_SelectionChanged(
        winrt::Windows::Foundation::IInspectable const&,
        winrt::Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const&)
    {
        switch (DurationComboBox().SelectedIndex())
        {
        case 1:
            exportSettings_.clipDuration = ClipDuration::Seconds90;
            break;
        case 2:
            exportSettings_.clipDuration = ClipDuration::Seconds120;
            break;
        default:
            exportSettings_.clipDuration = ClipDuration::Seconds60;
            break;
        }
    }

    void MainWindow::ResolutionComboBox_SelectionChanged(
        winrt::Windows::Foundation::IInspectable const&,
        winrt::Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const&)
    {
        exportSettings_.resolution = ResolutionComboBox().SelectedIndex() == 1
            ? Resolution::P1080
            : Resolution::P720;
    }

    void MainWindow::FrameRateComboBox_SelectionChanged(
        winrt::Windows::Foundation::IInspectable const&,
        winrt::Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const&)
    {
        exportSettings_.frameRate = FrameRateComboBox().SelectedIndex() == 1
            ? FrameRate::Fps60
            : FrameRate::Fps30;
    }

    bool MainWindow::setAudioTrack(
        std::filesystem::path path,
        std::wstring fileName,
        double durationSeconds)
    {
        if (path.empty() || !std::isfinite(durationSeconds) || durationSeconds <= 0.0 ||
            durationSeconds >= static_cast<double>(std::numeric_limits<std::int64_t>::max()))
        {
            showError("The MP3 duration is invalid.");
            return false;
        }

        exportSettings_.audioPath = std::move(path);
        exportSettings_.audioDurationSeconds = durationSeconds;
        audioFileName_ = std::move(fileName);
        updateDurationDisplay();
        return true;
    }

    void MainWindow::clearAudioTrack()
    {
        exportSettings_.audioPath.clear();
        exportSettings_.audioDurationSeconds.reset();
        audioFileName_.clear();
        updateDurationDisplay();
    }

    void MainWindow::updateDurationDisplay()
    {
        const bool hasAudio = !exportSettings_.audioPath.empty() &&
            exportSettings_.audioDurationSeconds.has_value();
        Mp3NameText().Text(hasAudio ? winrt::hstring{ audioFileName_ } : winrt::hstring{});
        RemoveMp3Button().Visibility(hasAudio
            ? winrt::Microsoft::UI::Xaml::Visibility::Visible
            : winrt::Microsoft::UI::Xaml::Visibility::Collapsed);
        DurationComboBox().IsEnabled(!hasAudio);
        DurationComboBox().Visibility(hasAudio
            ? winrt::Microsoft::UI::Xaml::Visibility::Collapsed
            : winrt::Microsoft::UI::Xaml::Visibility::Visible);
        Mp3DurationText().Visibility(hasAudio
            ? winrt::Microsoft::UI::Xaml::Visibility::Visible
            : winrt::Microsoft::UI::Xaml::Visibility::Collapsed);

        if (hasAudio)
        {
            const auto totalSeconds = static_cast<std::int64_t>(
                std::llround(*exportSettings_.audioDurationSeconds));
            const std::int64_t hours = totalSeconds / 3600;
            const std::int64_t minutes = (totalSeconds % 3600) / 60;
            const std::int64_t seconds = totalSeconds % 60;
            std::ostringstream text;
            text << "Full MP3 duration: ";
            if (hours > 0)
            {
                text << hours << ':' << std::setfill('0') << std::setw(2) << minutes
                     << ':' << std::setw(2) << seconds;
            }
            else
            {
                text << minutes << ':' << std::setfill('0') << std::setw(2) << seconds;
            }
            Mp3DurationText().Text(toHString(text.str()));
        }
            updateControlAvailability();
        }

        void MainWindow::updateControlAvailability()
        {
            const bool canEdit = !exporting_;
            LoadJsonButton().IsEnabled(canEdit);
            LoadMp3Button().IsEnabled(canEdit);
            RemoveMp3Button().IsEnabled(canEdit && !exportSettings_.audioPath.empty());
            DurationComboBox().IsEnabled(canEdit && exportSettings_.audioPath.empty());
            ResolutionComboBox().IsEnabled(canEdit);
            FrameRateComboBox().IsEnabled(canEdit);
            ExportButton().IsEnabled(canEdit && projectLoaded_);
            CancelExportButton().IsEnabled(exporting_);
        }

        void MainWindow::startPreview()
        {
            stopPreview();
            if (!projectLoaded_)
            {
                return;
            }

            auto state = std::make_shared<PreviewState>();
            previewState_ = state;
            const ShadertoyProject project = project_;
            PreviewStatusText().Text(L"Preparing shader preview...");
            previewThread_ = std::jthread([state, project](std::stop_token stopToken) {
                OpenGLShaderRenderer renderer;
                if (!renderer.initialize(project, PreviewWidth, PreviewHeight, 30))
                {
                    std::scoped_lock lock(state->mutex);
                    state->error = renderer.lastError();
                    return;
                }

                std::vector<std::uint8_t> pixels;
                const auto start = std::chrono::steady_clock::now();
                auto nextFrame = start;
                int frameIndex = 0;
                while (!stopToken.stop_requested())
                {
                    const double timeSeconds = std::chrono::duration<double>(
                        std::chrono::steady_clock::now() - start).count();
                    if (!renderer.renderFrame(timeSeconds, frameIndex++, pixels))
                    {
                        std::scoped_lock lock(state->mutex);
                        state->error = renderer.lastError();
                        return;
                    }
                    {
                        std::scoped_lock lock(state->mutex);
                        state->pixels = pixels;
                        ++state->frameSequence;
                    }
                    nextFrame += std::chrono::milliseconds(33);
                    while (!stopToken.stop_requested() && std::chrono::steady_clock::now() < nextFrame)
                    {
                        std::this_thread::sleep_for(std::chrono::milliseconds(2));
                    }
                }
            });
        }

        void MainWindow::stopPreview()
        {
            if (previewThread_.joinable())
            {
                previewThread_.request_stop();
                previewThread_.join();
            }
            previewState_.reset();
        }

        void MainWindow::applyPreviewPixels(std::vector<std::uint8_t> const& pixels)
        {
            const std::size_t expectedSize = static_cast<std::size_t>(PreviewWidth) * PreviewHeight * 4;
            if (pixels.size() != expectedSize)
            {
                return;
            }

            auto pixelBuffer = previewBitmap_.PixelBuffer();
            winrt::com_ptr<::Windows::Storage::Streams::IBufferByteAccess> bufferAccess;
            winrt::check_hresult(reinterpret_cast<IUnknown*>(winrt::get_abi(pixelBuffer))->QueryInterface(
                __uuidof(::Windows::Storage::Streams::IBufferByteAccess), bufferAccess.put_void()));
            BYTE* destination = nullptr;
            winrt::check_hresult(bufferAccess->Buffer(&destination));
            for (std::size_t offset = 0; offset < expectedSize; offset += 4)
            {
                destination[offset] = pixels[offset + 2];
                destination[offset + 1] = pixels[offset + 1];
                destination[offset + 2] = pixels[offset];
                destination[offset + 3] = pixels[offset + 3];
            }
            previewBitmap_.Invalidate();
            PreviewStatusText().Text(L"");
        }

        void MainWindow::updatePreview()
        {
            const auto state = previewState_;
            if (!state)
            {
                return;
            }

            std::vector<std::uint8_t> pixels;
            std::string error;
            std::uint64_t sequence = 0;
            {
                std::scoped_lock lock(state->mutex);
                sequence = state->frameSequence;
                if (sequence != displayedPreviewSequence_)
                {
                    pixels = state->pixels;
                }
                error = state->error;
            }
            if (!error.empty())
            {
                PreviewStatusText().Text(toHString("Preview unavailable: " + error));
                previewState_.reset();
                return;
            }
            if (!pixels.empty())
            {
                applyPreviewPixels(pixels);
                displayedPreviewSequence_ = sequence;
            }
        }

        void MainWindow::startExport(const std::filesystem::path& outputPath)
        {
            if (!projectLoaded_ || exporting_ || outputPath.empty())
            {
                return;
            }

            stopPreview();
            lastOutputPath_.clear();
            OpenVideoButton().Visibility(winrt::Microsoft::UI::Xaml::Visibility::Collapsed);
            OpenFolderButton().Visibility(winrt::Microsoft::UI::Xaml::Visibility::Collapsed);
            exportSettings_.outputPath = outputPath;
            exportRenderer_ = std::make_unique<OpenGLShaderRenderer>();
            exportEncoder_ = std::make_unique<FFmpegEncoder>();
            exportAudio_ = std::make_unique<FFmpegAudioSource>();
            exportPipeline_ = std::make_unique<ExportPipeline>(
                *exportRenderer_, *exportEncoder_, *exportAudio_);
            exportUiState_ = std::make_shared<ExportUiState>();
            exportUiState_->startTime = std::chrono::steady_clock::now();

            const int fps = exportSettings_.fps();
            auto state = exportUiState_;
            exportTask_.emplace(exportPipeline_->startAsync(project_, exportSettings_,
                [state, fps](ExportProgress const& progress) {
                    std::scoped_lock lock(state->mutex);
                    state->progress.frameIndex = progress.frameIndex;
                    state->progress.totalFrames = progress.totalFrames;
                    if (!progress.previewPixels.empty())
                    {
                        state->previewPixels = progress.previewPixels;
                    }
                }));

            exporting_ = true;
            ExportProgressBar().Value(0);
            ExportProgressBar().Visibility(winrt::Microsoft::UI::Xaml::Visibility::Visible);
            ExportProgressText().Text(L"Starting export...");
            ExportProgressText().Visibility(winrt::Microsoft::UI::Xaml::Visibility::Visible);
            CancelExportButton().Visibility(winrt::Microsoft::UI::Xaml::Visibility::Visible);
            ExportButton().Visibility(winrt::Microsoft::UI::Xaml::Visibility::Collapsed);
            StatusInfoBar().IsOpen(false);
            updateControlAvailability();
        }

        void MainWindow::finishExport()
        {
            if (!exportTask_)
            {
                return;
            }

            ExportStatus status = ExportStatus::Failed;
            std::string error;
            try
            {
                status = exportTask_->wait();
            }
            catch (std::exception const& exception)
            {
                error = exception.what();
            }

            if (status == ExportStatus::Failed && error.empty())
            {
                if (!exportEncoder_->lastError().empty())
                {
                    error = exportEncoder_->lastError();
                }
                else if (!exportRenderer_->lastError().empty())
                {
                    error = exportRenderer_->lastError();
                }
                else if (!exportAudio_->lastError().empty())
                {
                    error = exportAudio_->lastError();
                }
                else
                {
                    error = "The export failed. Check the shader and output file, then try again.";
                }
            }

            exporting_ = false;
            exportTask_.reset();
            exportPipeline_.reset();
            exportAudio_.reset();
            exportEncoder_.reset();
            exportRenderer_.reset();
            CancelExportButton().Visibility(winrt::Microsoft::UI::Xaml::Visibility::Collapsed);
            ExportButton().Visibility(winrt::Microsoft::UI::Xaml::Visibility::Visible);

            if (status == ExportStatus::Completed)
            {
                std::error_code fileError;
                if (!std::filesystem::is_regular_file(exportSettings_.outputPath, fileError) || fileError)
                {
                    showError("Export failed", "The encoder completed but the MP4 file was not created.");
                }
                else
                {
                    lastOutputPath_ = exportSettings_.outputPath;
                    OpenVideoButton().Visibility(winrt::Microsoft::UI::Xaml::Visibility::Visible);
                    OpenFolderButton().Visibility(winrt::Microsoft::UI::Xaml::Visibility::Visible);
                    StatusInfoBar().Severity(InfoBarSeverity::Success);
                    StatusInfoBar().Title(L"Export complete");
                    StatusInfoBar().Message(L"The MP4 video was exported successfully.");
                    StatusInfoBar().IsOpen(true);
                }
            }
            else if (status == ExportStatus::Cancelled)
            {
                ExportProgressText().Text(L"Export cancelled.");
                StatusInfoBar().IsOpen(false);
            }
            else
            {
                showError("Export failed", error);
            }

            exportUiState_.reset();
            updateControlAvailability();
            if (projectLoaded_)
            {
                startPreview();
            }
        }

        void MainWindow::UiTimer_Tick(
            winrt::Microsoft::UI::Dispatching::DispatcherQueueTimer const&,
            winrt::Windows::Foundation::IInspectable const&)
        {
            if (exporting_ && exportUiState_)
            {
                ExportProgress progress;
                std::vector<std::uint8_t> previewPixels;
                std::chrono::steady_clock::time_point startTime;
                {
                    std::scoped_lock lock(exportUiState_->mutex);
                    progress = exportUiState_->progress;
                    previewPixels = exportUiState_->previewPixels;
                    startTime = exportUiState_->startTime;
                }

                if (progress.totalFrames > 0)
                {
                    const double percent = 100.0 * progress.frameIndex / progress.totalFrames;
                    ExportProgressBar().Value(percent);
                    const double elapsed = std::chrono::duration<double>(
                        std::chrono::steady_clock::now() - startTime).count();
                    const double remaining = progress.frameIndex > 0
                        ? elapsed * (progress.totalFrames - progress.frameIndex) / progress.frameIndex
                        : 0.0;
                    std::ostringstream text;
                    text << std::fixed << std::setprecision(1) << percent << "% - Frame "
                         << progress.frameIndex << " of " << progress.totalFrames
                         << " - Estimated time remaining: " << static_cast<int>(remaining) << " s";
                    ExportProgressText().Text(toHString(text.str()));
                }
                if (!previewPixels.empty())
                {
                    applyPreviewPixels(previewPixels);
                }
                if (exportTask_ && exportTask_->isReady())
                {
                    finishExport();
                }
            }
            else
            {
                updatePreview();
            }
        }
}
