/**
 * @file Seeed_UI.h
 * @brief Optional lightweight retained-mode UI layer for Seeed_GFX.
 */
#ifndef SEEED_UI_H
#define SEEED_UI_H

#include "Seeed_GFX.h"

#include "ui/UiConfig.h"
#include "ui/UiStatus.h"
#include "ui/UiTypes.h"
#include "ui/UiEvent.h"
#include "ui/UiScreen.h"
#include "ui/UiFocusManager.h"
#include "ui/UiNavigator.h"
#include "ui/UiOverlayManager.h"
#include "ui/UiApplication.h"

#include "ui/theme/UiTheme.h"

#include "ui/input/UiRawEvent.h"
#include "ui/input/IUiInputSource.h"
#include "ui/input/UiEventQueue.h"
#include "ui/input/UiActionMap.h"
#include "ui/input/UiButtonScanner.h"
#include "ui/input/ButtonInput.h"
#include "ui/input/UiInputHub.h"
#include "ui/input/WioTerminalInput.h"
#include "ui/input/TouchInput.h"
#include "ui/input/EncoderInput.h"
#include "ui/input/SenseCAPWatcherInput.h"

#include "ui/render/UiCanvas.h"
#include "ui/render/UiDirtyRegion.h"
#include "ui/render/UiRenderScheduler.h"

#include "ui/widget/UiWidget.h"
#include "ui/widget/UiWidgets.h"
#include "ui/widget/UiImage.h"
#include "ui/layout/UiLayouts.h"

#include "ui/image/UiImageDecoder.h"
#include "ui/image/UiImageDecoders.h"

#endif
