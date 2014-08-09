/*  Copyright 2014 Mandible Games
    
    This file is part of Frames.
    
    Please see the COPYING file for detailed licensing information.
    
    Frames is dual-licensed software. It is available under both a
    commercial license, and also under the terms of the GNU General
    Public License as published by the Free Software Foundation, either
    version 3 of the License, or (at your option) any later version.

    Frames is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Frames.  If not, see <http://www.gnu.org/licenses/>. */

#include "FramesPCH.h"

#include "FramesLibrary.h"

#include <frames/frame.h>
#include <frames/mask.h>
#include <frames/sprite.h>
#include <frames/text.h>

#include "FramesConfigurationLocal.h"
#include "FramesConvert.h"
#include "FramesFrame.h"
#include "FramesManager.h"

UFramesLibrary::UFramesLibrary(const class FPostConstructInitializeProperties &PCIP)
  : Super(PCIP)
{
  
}

/*static*/ UFramesConfigurationLocal *UFramesLibrary::FramesConfigurationLocalCreate(UObject *WorldContextObject) {
  return new UFramesConfigurationLocal(FPostConstructInitializeProperties());
}

/*static*/ UFramesEnvironment *UFramesLibrary::FramesEnvironmentCreate(UObject *WorldContextObject, UFramesConfigurationLocal *Configuration) {
  UFramesEnvironment *env = new UFramesEnvironment(FPostConstructInitializeProperties());
  env->Initialize(Configuration->m_config);
  return env;
}

/*static*/ UFramesFrame *UFramesLibrary::FramesFrameCreate(UObject* WorldContextObject, UFramesLayout *Parent, const FString &Name) {
  return Cast<UFramesFrame>(FramesManager::Get().Convert(Frames::Frame::Create(Parent ? Parent->LayoutGet() : 0, Frames::detail::UE4Convert(Name))));
}

/*static*/ UFramesMask *UFramesLibrary::FramesMaskCreate(UObject* WorldContextObject, UFramesLayout *Parent, const FString &Name) {
  return Cast<UFramesMask>(FramesManager::Get().Convert(Frames::Mask::Create(Parent ? Parent->LayoutGet() : 0, Frames::detail::UE4Convert(Name))));
}

/*static*/ UFramesSprite *UFramesLibrary::FramesSpriteCreate(UObject* WorldContextObject, UFramesLayout *Parent, const FString &Name) {
  return Cast<UFramesSprite>(FramesManager::Get().Convert(Frames::Sprite::Create(Parent ? Parent->LayoutGet() : 0, Frames::detail::UE4Convert(Name))));
}

/*static*/ UFramesText *UFramesLibrary::FramesTextCreate(UObject* WorldContextObject, UFramesLayout *Parent, const FString &Name) {
  return Cast<UFramesText>(FramesManager::Get().Convert(Frames::Text::Create(Parent ? Parent->LayoutGet() : 0, Frames::detail::UE4Convert(Name))));
}
