#pragma once

#include "MainWindow.g.h"
#include "ExportSettings.h"
#include "ExportPipeline.h"
#include "OpenGLShaderRenderer.h"
#include "ShadertoyProject.h"

#include <winrt/Microsoft.UI.Xaml.Media.Imaging.h>

#include <chrono>
#include <memory>
#include <mutex>
#include <optional>
#include <filesystem>
#include <string>
#include <thread>
#include <vector>

class FFmpegAudioSource;
class FFmpegEncoder;

namespace winrt::VideoConverterForShadertoy::implementation
{
    struct MainWindow : MainWindowT<MainWindow>
    {
        MainWindow();
        ~MainWindow();

        winrt::fire_and_forget LoadJsonButton_Click(
            winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        winrt::fire_and_forget LoadMp3Button_Click(
            winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        winrt::fire_and_forget ExportButton_Click(
            winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void CancelExportButton_Click(
            winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void OpenVideoButton_Click(
            winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void OpenFolderButton_Click(
            winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void RemoveMp3Button_Click(
            winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void DurationComboBox_SelectionChanged(
            winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& args);
        void ResolutionComboBox_SelectionChanged(
            winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& args);
        void FrameRateComboBox_SelectionChanged(
            winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& args);

    private:
        void configureWindow();
        void showLoadedProject();
        void showNoProject();
        void showError(std::string const& message);
        void showError(std::string const& title, std::string const& message);
        void showWarnings(std::vector<std::string> const& warnings);
        bool setAudioTrack(std::filesystem::path path, std::wstring fileName, double durationSeconds);
        void clearAudioTrack();
        void updateDurationDisplay();
        void updateControlAvailability();
        void updatePreview();
        void applyPreviewPixels(std::vector<std::uint8_t> const& pixels);
        void startPreview();
        void stopPreview();
        void startExport(const std::filesystem::path& outputPath);
        void finishExport();
        void UiTimer_Tick(
            winrt::Microsoft::UI::Dispatching::DispatcherQueueTimer const& sender,
            winrt::Windows::Foundation::IInspectable const& args);

        struct PreviewState
        {
            std::mutex mutex;
            std::vector<std::uint8_t> pixels;
            std::string error;
            std::uint64_t frameSequence = 0;
        };

        struct ExportUiState
        {
            std::mutex mutex;
            ExportProgress progress;
            std::vector<std::uint8_t> previewPixels;
            std::chrono::steady_clock::time_point startTime;
        };

        ShadertoyProject project_;
        ExportSettings exportSettings_;
        std::wstring audioFileName_;
        std::filesystem::path lastOutputPath_;
        std::shared_ptr<PreviewState> previewState_;
        std::shared_ptr<ExportUiState> exportUiState_;
        std::jthread previewThread_;
        winrt::Microsoft::UI::Dispatching::DispatcherQueueTimer uiTimer_{ nullptr };
        winrt::Microsoft::UI::Xaml::Media::Imaging::WriteableBitmap previewBitmap_{ nullptr };
        std::unique_ptr<OpenGLShaderRenderer> exportRenderer_;
        std::unique_ptr<::FFmpegEncoder> exportEncoder_;
        std::unique_ptr<::FFmpegAudioSource> exportAudio_;
        std::unique_ptr<ExportPipeline> exportPipeline_;
        std::optional<ExportTask> exportTask_;
        std::uint64_t displayedPreviewSequence_ = 0;
        bool projectLoaded_ = false;
        bool exporting_ = false;
    };
}

namespace winrt::VideoConverterForShadertoy::factory_implementation
{
    struct MainWindow : MainWindowT<MainWindow, implementation::MainWindow>
    {
    };
}
