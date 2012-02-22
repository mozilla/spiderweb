/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Christopher Blizzard
 * <blizzard@mozilla.org>.  Portions created by the Initial Developer
 * are Copyright (C) 2001 the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef __nsGdkKeyUtils_h__
#define __nsGdkKeyUtils_h__

#include "nsEvent.h"
#include "nsTArray.h"

#include <gdk/gdk.h>

int      GdkKeyCodeToDOMKeyCode     (int aKeysym);
int      DOMKeyCodeToGdkKeyCode     (int aKeysym);
PRUint32 nsConvertCharCodeToUnicode (GdkEventKey* aEvent);

namespace mozilla {
namespace widget {

/**
 *  KeymapWrapper is a wrapper class of GdkKeymap.  GdkKeymap doesn't support
 *  all our needs, therefore, we need to access lower level APIs.
 *  But such code is usually complex and might be slow.  Against such issues,
 *  we should cache some information.
 *
 *  This class provides only static methods.  The methods is using internal
 *  singleton instance which is initialized by default GdkKeymap.  When the
 *  GdkKeymap is destroyed, the singleton instance will be destroyed.
 */

class KeymapWrapper
{
public:
    /**
     * Modifier is list of modifiers which we support in widget level.
     */
    enum Modifier {
        NOT_MODIFIER       = 0x0000,
        CAPS_LOCK          = 0x0001,
        NUM_LOCK           = 0x0002,
        SCROLL_LOCK        = 0x0004,
        SHIFT              = 0x0008,
        CTRL               = 0x0010,
        ALT                = 0x0020,
        SUPER              = 0x0040,
        HYPER              = 0x0080,
        META               = 0x0100,
        ALTGR              = 0x0200
    };

    /**
     * Modifiers is used for combination of Modifier.
     * E.g., |Modifiers modifiers = (SHIFT | CTRL);| means Shift and Ctrl.
     */
    typedef PRUint32 Modifiers;

protected:

    /**
     * GetInstance() returns a KeymapWrapper instance.
     *
     * @return                  A singleton instance of KeymapWrapper.
     */
    static KeymapWrapper* GetInstance();

    KeymapWrapper();
    ~KeymapWrapper();

    bool mInitialized;

    /**
     * Initializing methods.
     */
    void Init();
    void InitBySystemSettings();

    /**
     * mModifierKeys stores each hardware key information.
     */
    struct ModifierKey {
        guint mHardwareKeycode;
        guint mMask;

        ModifierKey(guint aHardwareKeycode) :
          mHardwareKeycode(aHardwareKeycode), mMask(0)
        {
        }
    };
    nsTArray<ModifierKey> mModifierKeys;

    /**
     * GetModifierKey() returns modifier key information of the hardware
     * keycode.  If the key isn't a modifier key, returns NULL.
     */
    ModifierKey* GetModifierKey(guint aHardwareKeycode);

    /**
     * mModifierMasks is bit masks for each modifier.  The index should be one
     * of ModifierIndex values.
     */
    enum ModifierIndex {
        INDEX_NUM_LOCK,
        INDEX_SCROLL_LOCK,
        INDEX_ALT,
        INDEX_SUPER,
        INDEX_HYPER,
        INDEX_META,
        INDEX_ALTGR,
        COUNT_OF_MODIFIER_INDEX
    };
    guint mModifierMasks[COUNT_OF_MODIFIER_INDEX];

    guint GetModifierMask(Modifier aModifier) const;

    /**
     * @param aGdkKeyval        A GDK defined modifier key value such as
     *                          GDK_Shift_L.
     * @return                  Returns Modifier values for aGdkKeyval.
     *                          If the given key code isn't a modifier key,
     *                          returns NOT_MODIFIER.
     */
    static Modifier GetModifierForGDKKeyval(guint aGdkKeyval);

#ifdef PR_LOGGING
    static const char* GetModifierName(Modifier aModifier);
#endif // PR_LOGGING

    /**
     * mGdkKeymap is a wrapped instance by this class.
     */
    GdkKeymap* mGdkKeymap;

    /**
     * Pointer of the singleton instance.
     */
    static KeymapWrapper* sInstance;

    /**
     * Signal handlers.
     */
    static void OnKeysChanged(GdkKeymap* aKeymap, KeymapWrapper* aKeymapWrapper);
    static void OnDestroyKeymap(KeymapWrapper* aKeymapWrapper,
                                GdkKeymap *aGdkKeymap);
};

} // namespace widget
} // namespace mozilla

#endif /* __nsGdkKeyUtils_h__ */
