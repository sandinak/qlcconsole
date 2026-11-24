/*
  Q Light Controller Plus
  windowspace_mac.mm

  Copyright (C) Branson Matheson

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
*/

#import <AppKit/AppKit.h>

#include <QGuiApplication>
#include <QWidget>

#include "windowspace.h"

bool WindowSpace::keepOnParentSpace(QWidget *w)
{
    // Off screen (the tests) a winId is no NSView.
    if (w == nullptr || QGuiApplication::platformName() != QLatin1String("cocoa"))
        return false;
    NSView *view = (__bridge NSView *)reinterpret_cast<void *>(w->winId());
    NSWindow *win = view != nil ? view.window : nil;
    if (win == nil)
        return false;
    win.collectionBehavior &= ~NSWindowCollectionBehaviorMoveToActiveSpace;
    return true;
}
