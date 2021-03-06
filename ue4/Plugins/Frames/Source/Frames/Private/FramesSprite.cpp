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
    along with Frames.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "FramesPCH.h"

#include "FramesSprite.h"

#include "FramesCompat.h"
#include "FramesConvert.h"

UFramesSprite::UFramesSprite(const FObjectInitializer &PCIP)
  : Super(PCIP)
{

}

UTexture2D *UFramesSprite::TextureGet() const {
  if (!ValidCheck()) return nullptr;

  return LoadObject<UTexture2D>(NULL, *Frames::detail::UE4Convert(FramesSpriteGet()->TextureGet()), NULL);
}

void UFramesSprite::TextureSet(UTexture2D *Texture) const {
  if (!ValidCheck()) return;

  return FramesSpriteGet()->TextureSet(Frames::detail::UE4Convert(Texture->GetFullName()));
}
