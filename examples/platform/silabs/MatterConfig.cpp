/*
 *
 *    Copyright (c) 2020 Project CHIP Authors
 *    Copyright (c) 2022 Silabs.
 *    All rights reserved.
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#include "AppConfig.h"
#include "BaseApplication.h"
#include <MatterConfig.h>
#include <cmsis_os2.h>

#include <mbedtls/platform.h>

#ifdef SL_WIFI
#include <platform/silabs/NetworkCommissioningWiFiDriver.h>
#include <platform/silabs/wifi/WifiInterface.h>

#if CHIP_CONFIG_ENABLE_ICD_SERVER
#include <platform/silabs/wifi/icd/WifiSleepManager.h> // nogncheck
#endif                                                 // CHIP_CONFIG_ENABLE_ICD_SERVER

// TODO: We shouldn't need any platform specific includes in this file
#if (defined(SLI_SI91X_MCU_INTERFACE) && SLI_SI91X_MCU_INTERFACE == 1)
#include <platform/silabs/SiWx917/SiWxPlatformInterface.h>
#endif // (defined(SLI_SI91X_MCU_INTERFACE) && SLI_SI91X_MCU_INTERFACE == 1)
#endif // SL_WIFI

#if PW_RPC_ENABLED
#include "Rpc.h"
#endif

#ifdef ENABLE_CHIP_SHELL
#include "MatterShell.h"
#endif

#ifdef HEAP_MONITORING
#include "MemMonitoring.h"
#endif

#include <crypto/CHIPCryptoPAL.h>
// If building with the EFR32-provided crypto backend, we can use the
// opaque keystore
#if CHIP_CRYPTO_PLATFORM && !(defined(SLI_SI91X_MCU_INTERFACE))
#include <platform/silabs/efr32/Efr32PsaOperationalKeystore.h>
static chip::DeviceLayer::Internal::Efr32PsaOperationalKeystore gOperationalKeystore;
#endif

#include <app/InteractionModelEngine.h>
#include <app/TimerDelegates.h>
#include <data-model-providers/codegen/Instance.h>
#include <headers/ProvisionManager.h>

#ifdef SL_MATTER_TEST_EVENT_TRIGGER_ENABLED
#include "SilabsTestEventTriggerDelegate.h" // nogncheck
#endif

#if CHIP_CONFIG_SYNCHRONOUS_REPORTS_ENABLED
#include <app/reporting/SynchronizedReportSchedulerImpl.h>
#else
#include <app/reporting/ReportSchedulerImpl.h>
#endif

#include <lib/support/BytesToHex.h>

#ifdef PERFORMANCE_TEST_ENABLED
#include <performance_test_commands.h>
#endif

#include <AppTask.h>

#include <DeviceInfoProviderImpl.h>
#include <app/server/Server.h>

#include <platform/silabs/platformAbstraction/SilabsPlatform.h>

#include <app/clusters/network-commissioning/network-commissioning.h>

/**********************************************************
 * Defines
 *********************************************************/

using namespace ::chip;
using namespace ::chip::app;
using namespace ::chip::Inet;
using namespace ::chip::DeviceLayer;
using namespace ::chip::Credentials;
using namespace chip::DeviceLayer::Silabs;

#ifdef SL_WIFI
Clusters::NetworkCommissioning::InstanceAndDriver<NetworkCommissioning::SlWiFiDriver> sWifiNetworkDriver(kRootEndpointId);
#endif /* SL_WIFI */

#if CHIP_ENABLE_OPENTHREAD
#include <inet/EndPointStateOpenThread.h>
#include <openthread/cli.h>
#include <openthread/dataset.h>
#include <openthread/error.h>
#include <openthread/heap.h>
#include <openthread/icmp6.h>
#include <openthread/instance.h>
#include <openthread/link.h>
#include <openthread/platform/openthread-system.h>
#include <openthread/tasklet.h>
#include <openthread/thread.h>

#include <platform/OpenThread/GenericNetworkCommissioningThreadDriver.h>

Clusters::NetworkCommissioning::InstanceAndDriver<NetworkCommissioning::GenericThreadDriver> sThreadNetworkDriver(kRootEndpointId);
// ================================================================================
// Matter Networking Callbacks
// ================================================================================
void LockOpenThreadTask(void)
{
    chip::DeviceLayer::ThreadStackMgr().LockThreadStack();
}

void UnlockOpenThreadTask(void)
{
    chip::DeviceLayer::ThreadStackMgr().UnlockThreadStack();
}

// ================================================================================
// SilabsMatterConfig Methods
// ================================================================================
CHIP_ERROR SilabsMatterConfig::InitOpenThread(void)
{
    ChipLogProgress(DeviceLayer, "Initializing OpenThread stack");
    ReturnErrorOnFailure(ThreadStackMgr().InitThreadStack());

#if CHIP_DEVICE_CONFIG_THREAD_FTD
    ReturnErrorOnFailure(ConnectivityMgr().SetThreadDeviceType(ConnectivityManager::kThreadDeviceType_Router));
#else // CHIP_DEVICE_CONFIG_THREAD_FTD
#if CHIP_CONFIG_ENABLE_ICD_SERVER
#if CHIP_DEVICE_CONFIG_THREAD_SSED
    ReturnErrorOnFailure(ConnectivityMgr().SetThreadDeviceType(ConnectivityManager::kThreadDeviceType_SynchronizedSleepyEndDevice));
#else
    ReturnErrorOnFailure(ConnectivityMgr().SetThreadDeviceType(ConnectivityManager::kThreadDeviceType_SleepyEndDevice));
#endif
#else  // CHIP_CONFIG_ENABLE_ICD_SERVER
    ReturnErrorOnFailure(ConnectivityMgr().SetThreadDeviceType(ConnectivityManager::kThreadDeviceType_MinimalEndDevice));
#endif // CHIP_CONFIG_ENABLE_ICD_SERVER
#endif // CHIP_DEVICE_CONFIG_THREAD_FTD

    sThreadNetworkDriver.Init();

    ChipLogProgress(DeviceLayer, "Starting OpenThread task");
    return ThreadStackMgrImpl().StartThreadTask();
}
#endif // CHIP_ENABLE_OPENTHREAD

namespace {

constexpr uint32_t kMainTaskStackSize = (1024 * 5);
// Task is dynamically allocated with max priority. This task gets deleted once the inits are completed.
constexpr osThreadAttr_t kMainTaskAttr = { .name       = "main",
                                           .attr_bits  = osThreadDetached,
                                           .cb_mem     = NULL,
                                           .cb_size    = 0U,
                                           .stack_mem  = NULL,
                                           .stack_size = kMainTaskStackSize,
#ifdef SLI_SI91X_MCU_INTERFACE
                                           .priority = osPriorityRealtime4
#else
                                           .priority = osPriorityRealtime7
#endif // SLI_SI91X_MCU_INTERFACE
};
osThreadId_t sMainTaskHandle;
static chip::DeviceLayer::DeviceInfoProviderImpl gExampleDeviceInfoProvider;

void ApplicationStart(void * unused)
{
    CHIP_ERROR err = SilabsMatterConfig::InitMatter(BLE_DEV_NAME);
    if (err != CHIP_NO_ERROR)
        appError(err);

    gExampleDeviceInfoProvider.SetStorageDelegate(&chip::Server::GetInstance().GetPersistentStorage());
    chip::DeviceLayer::SetDeviceInfoProvider(&gExampleDeviceInfoProvider);

    chip::DeviceLayer::PlatformMgr().LockChipStack();
    // Initialize device attestation config
    SetDeviceAttestationCredentialsProvider(&Provision::Manager::GetInstance().GetStorage());
    chip::DeviceLayer::PlatformMgr().UnlockChipStack();

    ChipLogProgress(DeviceLayer, "Starting App Task");
    err = AppTask::GetAppTask().StartAppTask();
    if (err != CHIP_NO_ERROR)
        appError(err);

    VerifyOrDie(osThreadTerminate(sMainTaskHandle) == osOK); // Deleting the main task should never fail.
    sMainTaskHandle = nullptr;
}
} // namespace

void SilabsMatterConfig::AppInit()
{
    GetPlatform().Init();
    sMainTaskHandle = osThreadNew(ApplicationStart, nullptr, &kMainTaskAttr);
    ChipLogProgress(DeviceLayer, "Starting scheduler");
    VerifyOrDie(sMainTaskHandle); // We can't proceed if the Main Task creation failed.

// Use sl_system for projects upgraded to 2025.6, identified by the presence of SL_CATALOG_CUSTOM_MAIN_PRESENT
#if defined(SL_CATALOG_CUSTOM_MAIN_PRESENT)
    GetPlatform().StartScheduler();

    // Should never get here.
    chip::Platform::MemoryShutdown();
    ChipLogError(DeviceLayer, "Start Scheduler Failed, Not enough RAM");
    appError(CHIP_ERROR_NO_MEMORY);
#endif // SL_CATALOG_CUSTOM_MAIN_PRESENT
}

CHIP_ERROR SilabsMatterConfig::InitMatter(const char * appName)
{
    using namespace chip::DeviceLayer::Silabs;

    ChipLogProgress(DeviceLayer, "==================================================");
    ChipLogProgress(DeviceLayer, "%s starting", appName);
    ChipLogProgress(DeviceLayer, "==================================================");

#if PW_RPC_ENABLED
    chip::rpc::Init();
#endif

#ifdef HEAP_MONITORING
    MemMonitoring::StartMonitor();
#endif

    //==============================================
    // Init Matter Stack
    //==============================================
    ChipLogProgress(DeviceLayer, "Init CHIP Stack");

#ifdef SL_WIFI
    // Init Chip memory management before the stack
    // Because OpenThread needs to use memory allocation during its Key operations, we initialize the memory management for thread
    // and set the allocation functions inside sl_ot_create_instance, which is called earlier by sl_system_init.
    mbedtls_platform_set_calloc_free(CHIPPlatformMemoryCalloc, CHIPPlatformMemoryFree);
    ReturnErrorOnFailure(chip::Platform::MemoryInit());
    ReturnErrorOnFailure(WifiInterface::GetInstance().InitWiFiStack());

#if CHIP_CONFIG_ENABLE_ICD_SERVER
    ReturnErrorOnFailure(WifiSleepManager::GetInstance().Init(&WifiInterface::GetInstance(), &WifiInterface::GetInstance()));
#endif // CHIP_CONFIG_ENABLE_ICD_SERVER
#endif // SL_WIFI

    ReturnErrorOnFailure(PlatformMgr().InitChipStack());

    chip::DeviceLayer::ConnectivityMgr().SetBLEDeviceName(appName);

    // Provision Manager
    Provision::Manager & provision = Provision::Manager::GetInstance();
    ReturnErrorOnFailure(provision.Init());
    SetDeviceInstanceInfoProvider(&provision.GetStorage());
    SetCommissionableDataProvider(&provision.GetStorage());
    ChipLogProgress(DeviceLayer, "Provision mode %s", provision.IsProvisionRequired() ? "ENABLED" : "disabled");

    // Create initParams with SDK example defaults here
    // TODO: replace with our own init param to avoid double allocation in examples
    static chip::CommonCaseDeviceServerInitParams initParams;

#if CHIP_ENABLE_OPENTHREAD
    ReturnErrorOnFailure(InitOpenThread());

    // Set up OpenThread configuration when OpenThread is included
    chip::Inet::EndPointStateOpenThread::OpenThreadEndpointInitParam nativeParams;
    nativeParams.lockCb                = LockOpenThreadTask;
    nativeParams.unlockCb              = UnlockOpenThreadTask;
    nativeParams.openThreadInstancePtr = chip::DeviceLayer::ThreadStackMgrImpl().OTInstance();
    initParams.endpointNativeParams    = static_cast<void *>(&nativeParams);
#endif
#ifdef SL_WIFI
    // This must be initialized after InitWiFiStack and InitChipStack which enable communication between the TA an M4
    // This is required for TA nvm access used by the sWifiNetworkDriver.
    ReturnErrorOnFailure(sWifiNetworkDriver.Init());
#endif

    // Stop Matter event handling while setting up resources
    chip::DeviceLayer::PlatformMgr().LockChipStack();

    // Report scheduler and timer delegate instance
    static DefaultTimerDelegate sTimerDelegate;
#if CHIP_CONFIG_SYNCHRONOUS_REPORTS_ENABLED
    static reporting::SynchronizedReportSchedulerImpl sReportScheduler(&sTimerDelegate);
#else
    static reporting::ReportSchedulerImpl sReportScheduler(&sTimerDelegate);
#endif

    initParams.reportScheduler = &sReportScheduler;

#ifdef SL_MATTER_TEST_EVENT_TRIGGER_ENABLED
    static SilabsTestEventTriggerDelegate sTestEventTriggerDelegate;
    sTestEventTriggerDelegate.Init(&provision.GetStorage());

    initParams.testEventTriggerDelegate = &sTestEventTriggerDelegate;
#endif // SL_MATTER_TEST_EVENT_TRIGGER_ENABLED

#if CHIP_CRYPTO_PLATFORM && !(defined(SLI_SI91X_MCU_INTERFACE))
    // When building with EFR32 crypto, use the opaque key store
    // instead of the default (insecure) one.
    gOperationalKeystore.Init();
    initParams.operationalKeystore = &gOperationalKeystore;
#endif

    // Initialize the remaining (not overridden) providers to the SDK example defaults
    ReturnErrorOnFailure(initParams.InitializeStaticResourcesBeforeServerInit());
    initParams.dataModelProvider = CodegenDataModelProviderInstance(initParams.persistentStorageDelegate);
    initParams.appDelegate       = &BaseApplication::sAppDelegate;
    // Init Matter Server and Start Event Loop
    CHIP_ERROR err = chip::Server::GetInstance().Init(initParams);

    chip::DeviceLayer::PlatformMgr().UnlockChipStack();

    ReturnErrorOnFailure(err);

    SILABS_LOG("Starting Platform Manager Event Loop");
    ReturnErrorOnFailure(PlatformMgr().StartEventLoopTask());

#ifdef ENABLE_CHIP_SHELL
    chip::startShellTask();
#endif

    return CHIP_NO_ERROR;
}

// ================================================================================
// FreeRTOS Callbacks
// ================================================================================
extern "C" void vApplicationIdleHook(void)
{
#if (SLI_SI91X_MCU_INTERFACE && CHIP_CONFIG_ENABLE_ICD_SERVER)
#ifdef SL_CATALOG_SIMPLE_BUTTON_PRESENT
    SiWxPlatformInterface::sl_si91x_btn_event_handler();
#endif // SL_CATALOG_SIMPLE_BUTTON_PRESENT
    SiWxPlatformInterface::sl_si91x_uart_power_requirement_handler();
#endif
}
