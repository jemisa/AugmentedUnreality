/*
Copyright 2016 Krzysztof Lis

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http ://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

#pragma once

#include "Object.h"
//class UAURSmoothingFilter;
#include "AURDriver.generated.h"

USTRUCT(BlueprintType)
struct FAURVideoFrame
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = AugmentedReality)
	FIntPoint FrameResolution;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = AugmentedReality)
	TArray<FColor> Image;

	FAURVideoFrame()
	{
		this->SetResolution(FIntPoint(1280, 720));
	}

	FAURVideoFrame(FIntPoint resolution)
		: FrameResolution(resolution)
	{
		this->SetResolution(resolution);
	}

	void SetResolution(FIntPoint resolution)
	{
		this->FrameResolution = resolution;
		this->Image.Init(FColor::MakeRandomColor(), this->GetImageSize());
	}

	/**
	 * Number of pixels.
	 */
	int32 GetImageSize() const
	{
		return this->FrameResolution.X * this->FrameResolution.Y;
	}

	uint8* GetDataPointerRaw() const
	{
		return (uint8*)this->Image.GetData();
	}
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAURDriverConnectionStatusChange, UAURDriver*, Driver);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAURDriverCameraParametersChange, UAURDriver*, Driver);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAURDriverCalibrationStatusChange, UAURDriver*, Driver);
//DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAURDriverViewpointTransformUpdate, FTransform, ViewpointTransform);

/**
 * Represents a way of connecting to a camera.
 */
UCLASS(Blueprintable, BlueprintType, Abstract)
class UAURDriver : public UObject
{
	GENERATED_BODY()

public:
	/** True if it should track markers and calculate camera position+rotation */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AugmentedReality)
	uint32 bPerformOrientationTracking : 1;

	/** Called when the connection to camera is established or lost */
	UPROPERTY(BlueprintAssignable)
	FAURDriverConnectionStatusChange OnVideoSourceStatusChange;

	/** Called when the resolution or FOV changes */
	UPROPERTY(BlueprintAssignable)
	FAURDriverCameraParametersChange OnCameraParametersChange;

	/** Called when calibration starts or ends */
	UPROPERTY(BlueprintAssignable)
	FAURDriverCalibrationStatusChange OnCalibrationStatusChange;

	/** Called when a new viewpoint (camera) position is measured by the tracker */
	//UPROPERTY(BlueprintAssignable)
	//FAURDriverViewpointTransformUpdate OnViewpointTransformUpdate;

	UAURDriver();

	/**
	 * Start capturing video, setup marker tracking.
	 */
	UFUNCTION(BlueprintCallable, Category = AugmentedReality)
	virtual void Initialize();

	/**
	* Provide an UWorld reference for time measurement.
	*/
	UFUNCTION(BlueprintCallable, Category = AugmentedReality)
	void SetWorld(UWorld* world_reference)
	{
		this->WorldReference = world_reference;
	}

	/**
	 * Some drivers may need constant updating
	 */
	UFUNCTION(BlueprintCallable, Category = AugmentedReality)
	virtual void Tick();

	/**
	 * Stop capturing video, release resources.
	 */
	UFUNCTION(BlueprintCallable, Category = AugmentedReality)
	virtual void Shutdown();

	UFUNCTION(BlueprintCallable, Category = AugmentedReality)
	virtual bool OpenDefaultVideoSource();

	// Is the camera connected and working.
	UFUNCTION(BlueprintCallable, Category = AugmentedReality)
	virtual bool IsConnected() const;

	/**
	 * True if the specific calibration file for this camera has been found,
	 * False if it uses default fallback file.
	 */
	UFUNCTION(BlueprintCallable, Category = AugmentedReality)
	virtual bool IsCalibrated() const;

	UFUNCTION(BlueprintCallable, Category = AugmentedReality)
	virtual bool IsCalibrationInProgress() const;

	/**
	 * 0.0 <= progress <= 1.0 - calibration in progress
	 */
	UFUNCTION(BlueprintCallable, Category = AugmentedReality)
	virtual float GetCalibrationProgress() const;

	/**
	 * Attempt to calibrate the camera by observing a known pattern from different
	 * viewpoints.
	 */
	UFUNCTION(BlueprintCallable, Category = AugmentedReality)
	virtual void StartCalibration();

	UFUNCTION(BlueprintCallable, Category = AugmentedReality)
	virtual void CancelCalibration();

	UFUNCTION(BlueprintCallable, Category = AugmentedReality)
	virtual FIntPoint GetResolution() const;

	UFUNCTION(BlueprintCallable, Category = AugmentedReality)
	virtual FVector2D GetFieldOfView() const;

	UFUNCTION(BlueprintCallable, Category = AugmentedReality)
	void GetCameraParameters(FIntPoint & camera_resolution, FVector2D field_of_view) const
	{
		camera_resolution = this->GetResolution();
		field_of_view = this->GetFieldOfView();
	}

	/**
	 * Returns a pointer to FAURVideoFrame containing the current camera frame.
	 * Do not delete the pointer.
	 * The data in this frame will not change.
	 * Call this again to get a pointer to the new frame.
	 * THE EXISTING FRAME POINTER BECOMES INVALID WHEN GetFrame IS CALLED (it may be processed by other thread).
	 * If you need to store the data, copy it before GetFrame().
	 */
	virtual FAURVideoFrame* GetFrame();

	/**
	 * @returns true if a new frame has been captured since the last GetFrame() call.
	 */
	UFUNCTION(BlueprintCallable, Category = AugmentedReality)
	virtual bool IsNewFrameAvailable() const;

	UFUNCTION(BlueprintCallable, Category = AugmentedReality)
	virtual FString GetDiagnosticText() const;

	// Start tracking a board - called by the static board list mechanism (RegisterBoardForTracking)
	virtual bool RegisterBoard(AAURMarkerBoardDefinitionBase* board_actor, bool use_as_viewpoint_origin = false);

	virtual void UnregisterBoard(AAURMarkerBoardDefinitionBase* board_actor);

	/**
		Registers the board to be tracked by the currently running AURDriver
		(if one is running) and any AURDriver created in the future.

		Boards for AR tracking may be placed on various unconnected actors,
		who do not have the reference to the currently used AURDriver,
		or due to initialization order there may be no currently running driver at all.

		Therefore boards will be put on a global list and when a driver is present,
		it will read the list and track the board.
	**/
	UFUNCTION(BlueprintCallable, Category = AugmentedReality)
	static void RegisterBoardForTracking(AAURMarkerBoardDefinitionBase* board_actor, bool use_as_viewpoint_origin = false);

	UFUNCTION(BlueprintCallable, Category = AugmentedReality)
	static void UnregisterBoardForTracking(AAURMarkerBoardDefinitionBase* board_actor);

protected:
	// Is the driver turned on
	uint32 bActive : 1;

	// Resolution of the frames used for image transfer
	FIntPoint FrameResolution;

	uint32 bCalibrationInProgress : 1;

	/** Reference to UWorld for time measurement */
	UWorld* WorldReference;

	static void EnsureDirExists(FString FilePath)
	{
		IPlatformFile & filesystem = FPlatformFileManager::Get().GetPlatformFile();
		filesystem.CreateDirectoryTree(*FPaths::GetPath(FilePath));
	}

private:
	struct BoardRegistration
	{
		AAURMarkerBoardDefinitionBase* Board;
		bool ViewpointOrigin;

		BoardRegistration(AAURMarkerBoardDefinitionBase* board_actor, bool use_as_viewpoint_origin = false)
			: Board(board_actor)
			, ViewpointOrigin(use_as_viewpoint_origin)
		{}

		// For TArray.AddUnique
		bool operator==(BoardRegistration const & other) const
		{
			return Board == other.Board && ViewpointOrigin == other.ViewpointOrigin;
		}
	};
	static TArray<BoardRegistration> RegisteredBoards;
	static UAURDriver* CurrentDriver;

	static void RegisterDriver(UAURDriver* driver);
	static void UnregisterDriver(UAURDriver* driver);
};
