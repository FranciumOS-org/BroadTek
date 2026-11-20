# bluetoothd preference overrides

Every two-level preference lookup in `/usr/sbin/bluetoothd`, extracted by
finding each call to the preferences singleton `0x1003943f2` and reading the
two `std::string` constructions that follow it: the **domain** and then the
**key**. Lookups go through vtable slot `*0x48` for a boolean and `*0x80`
for a number.

All of them read from `/Library/Preferences/com.apple.MobileBluetooth.debug.plist`,
as a sub-dictionary named by the domain. A boolean lookup compares the value
as an uppercase **string** -- `TRUE`/`YES`/`FALSE`/`NO` -- and treats anything
else, including a real plist boolean, as "not found". See LE_ADVERTISING.md.

```sh
sudo defaults write /Library/Preferences/com.apple.MobileBluetooth.debug \
    <domain> -dict-add <key> -string TRUE
sudo pkill bluetoothd
```

1055 call sites; 700 domain/key pairs decoded; the rest did not
match the two-leaq pattern and are not listed. A key appearing here means the
lookup exists, **not** that it does anything useful -- `SupportsMultipleAdvertising`
is read, logged and then ignored by the gate that matters.

## `A2DP` (11)

- `AACBitrate`
- `AACELDBitrate`
- `AdaptiveEQEnable`
- `Default48KHz`
- `DisableAAC`
- `DisableAACELD`
- `DisableDiagnosingLoop`
- `LoudnessCompensationEQEnable`
- `SBCBitpool`
- `SampleRate`
- `mediaFlushTimeoutMs`

## `AccessoryLogging` (7)

- `APLogCollectionEnabled`
- `AccessoryLogCrashPopupKey`
- `AccessoryLogDWRPopupKey`
- `CrashAnalyticsTestMode`
- `StoredLogAvailable`
- `StoredLogsRetrieved`
- `SuppressAllPopups`

## `AdvAgentList` (1)

- `LE`

## `ADVBUFF` (4)

- `DisableENADVBuffers`
- `OverrideAPWakeDebounceTimerSeconds`
- `TestRemoevAndClearOnBufferDestroy`
- `UseDebugTypes`

## `ADVDEBUG` (2)

- `FakeADVUnsettledStateCount`
- `FakeBusyErrorOnADVStartCount`

## `AE` (14)

- `DisableEnhancedConnections`
- `DisableEnhancedConnectionsV2`
- `DisableExtAdvertising`
- `DisableExtScans`
- `DisablePeriodicAdvSyncTranRecipient`
- `DisablePeriodicAdvSyncTranSender`
- `DisablePeriodicAdvertisingAdv`
- `DisablePeriodicAdvertisingSync`
- `DisableSpecSets`
- `EnableBackgroundExtendedAdvertising`
- `MaxAdvertisingBackgroundSessions`
- `MaxBackgroundAdvertisingTimeForSession`
- `MinAdvertisingInterval`
- `SetExtendedAdvertisingParametersV2`

## `AOPInterfaceManager` (1)

- `LastDramFetchInterval`

## `AoS` (1)

- `MaxRetryInFrames`

## `ATT` (1)

- `ResponseTimeout`

## `AutomaticIEDKey` (1)

- `DefaultAutoIEDOff`

## `BBFC` (3)

- `EnableEstimateScanRequestTxDeniedByCoex`
- `EnforceBBFCScanPrioritization`
- `ScanTimeOut`

## `BiAoS` (1)

- `bypassHAoSMonitor`

## `BLESCAN` (28)

- `ApplyRSSIOffset`
- `DisablePrioritizationOverBT`
- `EnforcePrioritization`
- `FWStatsScan`
- `Interval`
- `IsMRCScanEnableOverride`
- `IsMRCScanEnableOverrideValue`
- `IsScanCompensationDisabled`
- `IsScanCoreCompensated`
- `IsScanCoreConcurrent`
- `IsScanCoreDebug`
- `IsScanCoreELNAOn`
- `IsScanParamsOverriden`
- `IsScanParamsPhyOverride`
- `IsScanParamsWitheLNAOnandeLNABypassScanCoreOverriden`
- `PrioritizationOverWiFiEnabled`
- `ScanCoreCoexQueryPeriod`
- `ScanCoreCompensationPercent`
- `ScanWindowSOSBeaconActivateScan`
- `ScanWindowSOSBeaconPrecisionFindRequest`
- `WindowMainCoreELNABypass`
- `WindowMainCoreELNAOn`
- `WindowScanCore`
- `WindowScanCoreELNABypass`
- `WindowScanCoreELNAOn`
- `enableMatchingRuleAndAddressBatched`
- `forcePassiveScan`
- `forcePassiveScanEnable`

## `BluetoothAutomationModeKey` (1)

- `DisableBluetoothUIServer`

## `BluetoothTTR` (4)

- `DeviceNameReadErrorTTRTriggeredThisBuild`
- `HighDutyScanTTRTriggeredThisBuild`
- `WakeLimitABCTriggeredThisBuild`
- `WakeLimitPerHour`

## `BSM` (1)

- `DebounceOverride`

## `BT` (2)

- `EraseAllPairedAndCachedDevice`
- `LogTimesyncReference`

## `BT Encryption Key Size Downgrade` (1)

- `1031681`

## `BTAudioHALPlugin` (3)

- `BTAudioDebugEnableUWBSStereoTest`
- `BTAudioHALEnableAudioLogging`
- `EnableAudioCaptures`

## `BTHALAudioPlugin` (1)

- `BypassAllowlist`

## `BtMetricsCache` (4)

- `BTChipBootupTimeStart`
- `BtPowerOnDuration`
- `FirmwareErrorCount`
- `LastBtPowerOnTimeStamp`

## `BTSessionManager` (1)

- `StateDumpHandleMapLogging`

## `BTUCM` (2)

- `BTUCMForceFacetimeWifiCoexOverride`
- `DefaultWifiCoexPriority`

## `CarPlay` (1)

- `enableLeCarPlay`

## `CATT` (6)

- `AllowAppleDevices`
- `AudioSinkSdp`
- `DisableClassicGraceTimer`
- `DisableLEGraceTimer`
- `DisableSdp`
- `ForceDisableAll`

## `ChipBoot` (1)

- `DisableInitChipSimpleFast`

## `CoEx` (1)

- `EnableDebugCounters`

## `CoexConnectionScanParameters` (3)

- `CoexConnectionScanIntervalWithAudio`
- `CoexConnectionScanIntervalWithoutAudio`
- `CoexConnectionScanTimeout`

## `COEXDEBUG` (5)

- `Enabled`
- `UCMWifiStateBitmap`
- `wifiStateBand`
- `wifiStateCritical2_4`
- `wifiStatePower`

## `CoexManager` (1)

- `Disable`

## `com.apple.bluetooth.internalAudioTool` (1)

- `BTHALAudioPlugin`

## `CONNECTION` (17)

- `ForceConnectionDelay`
- `LeConnectionDCKDisconnectionDisabledToStageOneValue`
- `LeConnectionDCKDisconnectionStageOneToDisabledValue`
- `LeConnectionDCKTimeDurationFromDisabledToStageOneValue`
- `LeConnectionDCKTimeDurationFromStageOneToDisabledValue`
- `LeConnectionHomeDisconnectionDisabledToStageOneValue`
- `LeConnectionHomeDisconnectionStageOneToDisabledValue`
- `LeConnectionHomeTimeDurationFromDisabledToStageOneValue`
- `LeConnectionHomeTimeDurationFromStageOneToDisabledValue`
- `LeConnectionMagnetDisabledToStageOneValue`
- `LeConnectionMagnetRSSIThresholdStageOne`
- `LeConnectionMagnetStageOneToDisabledValue`
- `LeConnectionMagnetTimeDurationFromDisabledToStageOneValue`
- `LeConnectionMagnetTimeDurationFromStageOneToDisabledValue`
- `LeConnectionManagerConnectionScansDefaultDuration`
- `LeConnectionThirdPartyAppDisabledToStageOneValue`
- `LeConnectionThirdPartyAppStageOneToDisabledValue`

## `CountryCode` (1)

- `UseRegulatoryDomain`

## `CT` (1)

- `UseCTPrefixForCTCrypto`

## `Desense` (7)

- `DesenseAverageSeconds`
- `DesenseDetectThreshold`
- `DesenseHysteresisValue`
- `ForceDesense`
- `MagnetStatsLogging`
- `NFDebugLogging`
- `TurnOffBTCoreDiversity`

## `Device` (18)

- `AllowDramWake`
- `ClassOfDevice`
- `EnableHDR`
- `EnableHRB`
- `GazeOnHeadOverride`
- `GazePresentOverride`
- `HDRForcePkt`
- `HDRPktTypeMask`
- `MaskCODandNameAsAppleWatch`
- `MaskLocalNameAsAppleWatch`
- `MaxConnUpdateInst`
- `NewCompanyId`
- `NewSubVersion`
- `OverrideCompanyId`
- `RAWHCICMD`
- `enableSDPPrint`
- `forceSupportHRB`
- `wipeIDCache`

## `DispatcherWatchdog` (1)

- `enableCrash`

## `Enable FW Logs` (1)

- `FWEnhancedLoggingEnable`

## `ENScanOverrides` (11)

- `ENEnableAdaptiveAPWakeScheduling`
- `ENOverrideExtraScanTimeInMs`
- `ENOverrideMaxAllowedRSSILevel`
- `ENOverrideMaxDailyShortAPWakes`
- `ENOverrideNormalAPWakeDeltaInSeconds`
- `ENOverrideNumOfSightingsHighThreshold`
- `ENOverrideRSSIHighThreshold`
- `ENOverrideScanWithDuplicatesDurationA2DPWifi2GInMS`
- `ENOverrideScanWithDuplicatesDurationInMS`
- `ENOverrideShortAPWakeDeltaInSeconds`
- `ENOverrideStationaryDurationInSeconds`

## `ePA` (7)

- `DisableA2DP`
- `DisableHFP`
- `DisableMagnetEPA`
- `DisableReTxEPA`
- `DisableThresholdEPA`
- `ePAThreshold`
- `iPAMaxThreshold`

## `FastConnect` (2)

- `DisableFastConnect`
- `DisableFastConnectAll`

## `FastR1ScanInterval` (1)

- `interval`

## `FastR1ScanType` (1)

- `scanType`

## `Filippo` (7)

- `ForceISM24`
- `ForceUnii1`
- `ForceUnii3`
- `ForceUnii5a`
- `ForceUnii5b`
- `ForceUnii5c`
- `ForceUnii5d`

## `FindMy` (1)

- `ForceCorruptLTK`

## `FORCED` (1)

- `NOT FORCED`

## `FrequentAPWakeup` (1)

- `WakePacketTypeVSE`

## `fw` (1)

- `TraceMode`

## `FWStreamLogging` (2)

- `FWCoreDumpEnable`
- `FWEnhancedLoggingEnable`

## `GATT` (4)

- `DUMMY_DIS`
- `DUMMY_DIS_NAME`
- `MTU`
- `StartInitSequenceOnConnectionComplete`

## `Handsfree` (1)

- `VVMSleep`

## `hci` (1)

- `AssertOnFlowErrors`

## `HFP` (7)

- `BTAudioDebugEnable2EV5Test`
- `DisableUWBS`
- `DisableUWBSStereo`
- `DisableWBS`
- `HideGeneralRevision`
- `Jitter`
- `audioFeedback`

## `HID` (4)

- `EnableGamepads`
- `EnableHIDHighPriority`
- `HIDLatencyAnomalyThreshold`
- `TestABC`

## `HRB` (2)

- `HRBBand`
- `SwitchToHRB`

## `HWRepair` (1)

- `BlueToolBTResult`

## `IAP` (1)

- `enableIAP`

## `kCBMsgArgs` (1)

- `kCBMsgId`

## `LE` (207)

- `AEAuxPhyMask`
- `AEMaxAuxOffset`
- `AEMaxChanins`
- `AEMaxScanBuffers`
- `AElengthAllowed`
- `AdvDenyListEnabled`
- `AdvRSSIOffset`
- `AllowAllConnectionParameterUpdates`
- `AllowConnScanPrioritizationAlways`
- `AllowFindMyActionConnScanPrioritization`
- `AllowFindMyActionTxPowerIncreaseLEConnecting`
- `AppRestorationEnable`
- `AppleExtendedAdvertisingReport`
- `CSSetParam_RTTAAOnlyN`
- `CSSetParam_RTTCapabilies`
- `CSSetParam_RTTRandomPayloadN`
- `CSSetParam_RTTSoundingN`
- `CSSetParam_TSWTime`
- `CSSetParam_csSyncPhysSupported`
- `CSSetParam_enable`
- `CSSetParam_maxAntennaPathsSupported`
- `CSSetParam_maxProceduresSupported`
- `CSSetParam_numAntennaSupported`
- `CSSetParam_numConfigSupported`
- `CSSetParam_optionalModesSupported`
- `CSSetParam_optionalNadmRandomCap`
- `CSSetParam_optionalNadmSoundingCap`
- `CSSetParam_optionalSubfeaturesSupported`
- `CSSetParam_optionalTFCSTimesSupported`
- `CSSetParam_optionalTIP1TimesSupported`
- `CSSetParam_optionalTIP2TimesSupported`
- `CSSetParam_optionalTPMTimesSupported`
- `CSSetParam_rolesSupported`
- `CloudPair`
- `ConfigureCreateConnectionEnabled`
- `ConnRSSIStageOneDigitalCarKey`
- `ConnRSSIStageOneDigitalHomeKey`
- `ConnRSSIStageOneExternal_Lock`
- `ConnRSSIStageOneFindMy`
- `ConnRSSIStageOneLECarPlay`
- `ConnRSSIStageOneMagnet`
- `ConnRSSIStageOneThirdPartyApp`
- `ConnScanTimeoutExtendPercent`
- `ConnectionDenyListCountThresholdOverride`
- `ConnectionDenyListIntervalSecondsOverride`
- `ConnectionDenyListTimeoutSecondsOverride`
- `ConnectionGracePeriod`
- `ConnectionScanAllowListEnabled`
- `ConnectionScanDenyListEnabled`
- `ConnectionScanInterval`
- `ConnectionScanTimeoutOverride`
- `ConnectionScanWindow`
- `CrashOnAdvertisingWatchdog`
- `DataLength`
- `DebugAllowedList`
- `DebugOverrideAutoReconnectEnabled`
- `DebugOverrideConnectionMaxCELength`
- `DebugOverrideConnectionMaxCIFrames`
- `DebugOverrideConnectionMinCELength`
- `DebugOverrideConnectionMinCIFrames`
- `DebugOverrideConnectionSupervisionTimeout`
- `DefaultConnectionInterval`
- `DeviceFastLeConnectionClientTimeoutSeconds`
- `DeviceFastLeConnectionForceAllow`
- `DisableADVBuffers`
- `DisableConnectionParameterUpdates`
- `DisableH7`
- `DisableLEConnectionPrioritization`
- `DisableMatchBuffers`
- `DisableSSP`
- `DisableSensorTrack`
- `DisallowBluetoothUsageNotification`
- `DisallowConfigureSynchronizeEvent`
- `DisallowControllerBTClockConfigure`
- `DisallowPhyStatisticsUsageNotification`
- `DisallowRSSIConnectionStatisticAndInterval`
- `DoNotEnforceAllowedList`
- `ENForceCustomAdvInterval`
- `ENScanWithDuplicateEnable`
- `EnableAdjustLEInstantFactor`
- `EnableGetExtraBTCSPhyLogs`
- `EnableGetExtraBTCSPhyLogsCustomValues`
- `EnableGetExtraBTCSPhyLogsCustomValuesAtInit`
- `EnablePhyStatsFindMy`
- `EnableRSSIDetectionDebugging`
- `EnableRSSIDetectionDebuggingUseCase`
- `EnableScanAgentPrints`
- `EnableXPCLog`
- `EnforceUseCaseRequiredList`
- `FastConnectionOverrideInstance`
- `FastLEConnectionForceFakeLocalControllerInfo`
- `FeatureVendorList`
- `ForceBoolIOCaps`
- `ForceLatencyStats`
- `ForcePhyStats`
- `ForceRSSIDetectionCMCheckLogic`
- `ForceRSSIDetectionCM_AND_Mask`
- `ForceRSSIDetectionCM_AND_Mask2`
- `ForceRSSIDetectionCM_XOR_Mask`
- `ForceRSSIDetectionCloser`
- `ForceRSSIDetectionDebug`
- `ForceRSSIDetectionEnable`
- `ForceRSSIDetectionFurther`
- `ForceRSSIDetectionMaskDecisionLogic`
- `ForceRSSIDetectionVar1`
- `ForceRSSIDetectionVar2`
- `ForceRSSIDetectionWindow`
- `ForceReplyH7`
- `ForceRssiThresholdOrderType19`
- `GlobalMinRSSIConnection`
- `IgnoreControllerConnectionCompleteOnCancel`
- `IncludeHiddenLEFeatureVendors`
- `InputCaps`
- `IsConnectionScanOverriden`
- `L2CAPMaxPendingPacketsLength`
- `LE2MPhyDisabled`
- `LEInstantFactorMacAU`
- `LESecuredConnectionDebugKeys`
- `LeConnectionLatencyHighMaxInterval`
- `LeConnectionLatencyHighMinInterval`
- `LeConnectionLatencyHighSupervisionTimeout`
- `LeConnectionLatencyLowMaxInterval`
- `LeConnectionLatencyLowMinInterval`
- `LeConnectionLatencyLowSupervisionTimeout`
- `LeConnectionLatencyMediumMaxInterval`
- `LeConnectionLatencyMediumMinInterval`
- `LeConnectionLatencyMediumSupervisionTimeout`
- `LeConnectionLatencyVeryHighMaxInterval`
- `LeConnectionLatencyVeryHighMinInterval`
- `LeConnectionLatencyVeryHighSupervisionTimeout`
- `LeConnectionMaxIntervalDigitalID`
- `LeConnectionMaxIntervalFindMy`
- `LeConnectionMaxIntervalMIDIv2InFrames`
- `LeConnectionMinIntervalDigitalID`
- `LeConnectionMinIntervalFindMy`
- `LeConnectionMinIntervalMIDIv2InFrames`
- `LeCsPhyLogsCmdOpCode`
- `LeCsPhyLogsCmdParamsSize`
- `LeCsPhyLogsDestCpuId`
- `LeCsPhyLogsHdrFlags`
- `LeCsPhyLogsPadding`
- `LeCsPhyLogsPhyStatus`
- `LeCsSetSchedulingConnEventCount`
- `LeCsSetSchedulingEventInterval`
- `LeCsSetSchedulingOffsetMax`
- `LeCsSetSchedulingOffsetMin`
- `LeCsSetSchedulingShouldReject`
- `LeCsSetSchedulingShouldUse`
- `LeCsSetSchedulingSubeventInterval`
- `LeCsSetSchedulingSubeventLengthMax`
- `LeCsSetSchedulingSubeventsPerEvents`
- `LeMaxCEDigitalID`
- `LeMaxCEMIDIv2InTimeslots`
- `LeMinCEDigitalID`
- `LeMinCEMIDIv2InTimeslots`
- `LeScanAgentDebugClearAdvDataCache`
- `MRCConfigForExtendedRangeUseCase`
- `MagnetTimeoutABCLog`
- `MaxConnPerBundleID`
- `MaxConnPerBundleIDFromEndConnectingClient`
- `MaxConnPerBundleIDFromFindMyClient`
- `MaxFilterAcceptListSize`
- `MaxUnpairedDatabasesCached`
- `MinRSSIConnection`
- `MinRSSIConnectionEnabled`
- `NumDisconnectionsFromDisabledToStageOne`
- `NumDisconnectionsFromStageOneToDisabled`
- `OutputCaps`
- `OverrideConnectionIntervalFindMy`
- `OverrideInitiatorKeys`
- `OverrideMaxLEPairedDevices`
- `OverrideMaxLESyncedDevices`
- `OverrideMrcEnable`
- `OverrideResponderKeys`
- `OverrideSupportLEAdvTxPowerIncrease`
- `OverrideSupportLEConnTxPowerIncrease`
- `OverrideTxPowerAuxConnectReq`
- `OverrideTxPowerConnectInd`
- `OverrideUsePublicAddress`
- `PersistUserKilledApplications`
- `PowerTagV2ForceEnableFeatureSet`
- `PowerTagV2ForceHandleEnable`
- `PowerTagV2ForceHandleValue`
- `SRPipeMaxConInt`
- `SRPipeMinConInt`
- `SRPipeSpecialParamsOff`
- `ScanAllowListEnabled`
- `ScanCoreSupporteLNAandeLNABypassInSingleWindow`
- `ScanDenyListEnabled`
- `SupportsChannelSounding`
- `SupportsChannelSoundingBasedOnRegion`
- `SupportsChannelSoundingPhyLog`
- `SupportsMultipleAdvertising`
- `SupportsOutboxSoftwareUpdateAdvertising`
- `SupportsScalablePipes`
- `TimeDurationFromDisabledToStageOne`
- `TimeDurationFromStageOneToDisabled`
- `advAllowListEnabled`
- `enableLELRInSetDefaultPhy`
- `enableLEPowerControl`
- `enableLeCsSetScheduling`
- `forceEnableMRC`
- `forceHostSlowDownLEScanOnACLProtectMode`
- `forceLEConnectionTxPowerEPA`
- `overridePrivateAddressRotationInterval`
- `sendFindMyConnectionUpdateAnyTime`
- `supportsNCSecondaryAdvInstance`

## `LEA` (1)

- `IgnoreMFi`

## `LeBroadcaster` (19)

- `AdvStatsCanWakeUpAP1`
- `AdvStatsCanWakeUpAP2`
- `AdvStatsEnable1`
- `AdvStatsEnable2`
- `AdvStatsInstance1`
- `AdvStatsInstance2`
- `AdvStatsPeriod1`
- `AdvStatsPeriod2`
- `AdvStatsReset1`
- `AdvStatsReset2`
- `AdvertisementInterval`
- `ConnectableAdvInt`
- `ExtAdvInt`
- `HoldStablePrivateAddressThreshold`
- `LeExtendStablePrivateAddressInterval`
- `NonConnectableAdvInt`
- `NonConnectableSecondaryAdvInt`
- `ObjectDiscoveryAdvInt`
- `SwupAdvInterval`

## `LEConnectionScanAgentList` (1)

- `LE`

## `LeDeviceCache` (4)

- `MaxConnectedOnDisk`
- `MaxSeenOnDisk`
- `NumDevicesInMemory`
- `WipeNameOrigin`

## `LeObserver` (5)

- `OverrideContactTracingLowDutyScanDurationSeconds`
- `OverrideContactTracingLowDutyScanIntervalSeconds`
- `OverrideContactTracingScanDeltaSeconds`
- `OverrideContactTracingScanDurationA2DPWifi2GSeconds`
- `OverrideContactTracingScanDurationSeconds`

## `LESCAN` (1)

- `MaxReportsInMap`

## `LEScanAgentList` (1)

- `LE`

## `LinkAdaptiveBitrateAAC` (4)

- `bitrateCount`
- `configs`
- `rates`
- `thresholds`

## `LOCATION` (1)

- `RefreshTime`

## `LowEnergyLongRange` (1)

- `LowEnergyLongRangeDisable`

## `LowPowerCoreScan` (3)

- `DisableLEConnectionStatsV1`
- `DisableLowPowerCoreScan`
- `DisableLowPowerCoreScanWithLimitedRSSIReading`

## `LPM` (15)

- `ConfigFlag`
- `ConfigFlagAdv`
- `Enable`
- `EnableCentauriDebug`
- `EnableHCI`
- `EnableHostDebugLogging`
- `EnableHostWakeTxAdvDiag`
- `FactoryMode`
- `HowManyKeys`
- `LocalBDAddr`
- `LoggingMode`
- `ScanInterval`
- `ScanWindow`
- `TimestampOverride`
- `WhiteGloveMode`

## `MacStatusNotificationIntervalMS` (1)

- `MacStatusNotificationIntervalMS`

## `Magnet` (20)

- `AllowPeripheralModeSupport`
- `AssertUpgrade`
- `DisableClassic`
- `DisableFlush`
- `DisableMagnetSetLatency`
- `EnableAWDL`
- `EnableAutomaticPairing`
- `EnableCrossLayerLogs`
- `EnablePerPacketLogging`
- `EnableRSSIHistory`
- `EnableRSSIMonitoring`
- `EnableSkywalkDataCRC`
- `EnableSkywalkDataLogs`
- `ForceERTMOnNonReliablePipes`
- `ForceMaxLatency`
- `ForcedClassicRSSIValue`
- `ForcedLERSSIValue`
- `HistorySize`
- `RSSIMinSwitchThreshold`
- `enableTSEverywhere`

## `ManagerStateWatchdog` (1)

- `WatchdogDuration`

## `MatchTable` (4)

- `ADPDBufferDupClearTimeoutSeconds`
- `ADPDBufferDupNumEntries`
- `NearbyInfoBufferDupClearTimeoutSeconds`
- `NearbyInfoBufferDupNumEntries`

## `MediumR1ScanInterval` (1)

- `interval`

## `MobileAsset` (1)

- `EnablePeriodicCheckForNewerVersion`

## `MRC` (2)

- `DisableMRC`
- `ForceGlobalMRCLogs`

## `Pairing` (2)

- `DisableH7`
- `ForceReplyH7`

## `Pipe` (3)

- `RxSlotSize`
- `TxSlotSize`
- `overrideMTUSize`

## `POWER` (2)

- `ForceWakeScanConfig`
- `ForceWakeScanForClient`

## `PTS` (2)

- `AVRCPControllerHack`
- `PhoneNumberHack`

## `Scan` (2)

- `DisableMatchTableConfigV2`
- `DisableMatchTableConfigV3`

## `ScanCore` (6)

- `EnableScanCore`
- `IntervalConnection`
- `IsScanCoreConnectionOverriden`
- `IsScanCoreEnabledConnectionScan`
- `WindowConnection`
- `eLNAConnection`

## `ScanCorePageScan` (9)

- `ScanCorePageScanAuxScanDurationInMs`
- `ScanCorePageScanDebugElnaMode`
- `ScanCorePageScanDebugEnabled`
- `ScanCorePageScanEnableAuxScan`
- `ScanCorePageScanInterlaced`
- `ScanCorePageScanInterval`
- `ScanCorePageScanPreemptAuxScan`
- `ScanCorePageScanUseAuxScanParams`
- `ScanCorePageScanWindow`

## `SCANDEBUG` (4)

- `DebugControllerConfigDispatchSequence`
- `DebugScanConfigHeySiri`
- `DebugScanConfigSequence`
- `ForceDispatchInsteadOfSerialConfig`

## `ScanEnhancements` (3)

- `DisableRemoveMultipleDupliateEntries`
- `DisableScanEnhancements`
- `KeepDupFilterWhenScanEnable`

## `SDPMirroring` (1)

- `DisableSDPMirroring`

## `Sentinel` (2)

- `IsFindMyWatchCoexHandled`
- `LEConnection`

## `Signposts` (1)

- `Enabled`

## `SMP` (1)

- `DisableCTKD`

## `SNR-MRC` (1)

- `DisableSNRMRC`

## `Stack` (7)

- `IOTDataCollection`
- `PacketAggregation`
- `RestrictRoleSwitchOnOutogingConnection`
- `disableClassicSecureConnections`
- `disableConnUpdateWAR`
- `disableWriteCODWAR`
- `force1M`

## `StackManager::initTransport` (1)

- `StackManager::initTransport`

## `StackSettings` (1)

- `HFServiceEnable`

## `Tags` (1)

- `AllowEveryone`

## `TTRUI` (1)

- `Disable`

## `TTRUIDisabledByUser` (1)

- `Disable`

## `UniAoS` (4)

- `IsmBandFactor`
- `NakThreshold`
- `ReTxThreshold`
- `ignoreFlush`

## `VoiceCommand` (1)

- `DelayInMsec`

## `VSCScanParams` (1)

- `DisableVSCScanParams`

## `WiAP` (1)

- `enableSink`

## `WillSleep` (1)

- `requiresWillSleepWorkaround`

## `Zone` (1)

- `CuffoffRSSIValue`

