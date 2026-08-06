/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
*  Copyright (C) 2023 - 2026 Oxsomi / Nielsbishere (Niels Brunekreef)
*
*  This program is free software: you can redistribute it and/or modify
*  it under the terms of the GNU General Public License as published by
*  the Free Software Foundation, either version 3 of the License, or
*  (at your option) any later version.
*
*  This program is distributed in the hope that it will be useful,
*  but WITHOUT ANY WARRANTY; without even the implied warranty of
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*  GNU General Public License for more details.
*
*  You should have received a copy of the GNU General Public License
*  along with this program. If not, see https://github.com/Oxsomi/core3/blob/main/LICENSE.
*  Be aware that GPL3 requires closed source products to be GPL3 too if released to the public.
*  To prevent this a separate license will have to be requested at contact@osomi.net for a premium;
*  This is called dual licensing.
*/

//platforms/android/OxC3Activity.java

package net.osomi.nativeactivity;

import android.content.pm.ActivityInfo;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.app.NativeActivity;
import android.content.Context;
import android.util.Log;
import android.view.KeyCharacterMap;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.PointerIcon;
import android.view.inputmethod.InputMethodManager;
import android.widget.EditText;
import android.text.*;

public class OxC3Activity extends NativeActivity {

	private static EditText inputField;		//Show soft keyboard if requested from OxC3

	public native void onTypeChar(String input);

	@Override
	protected void onCreate(Bundle savedInstanceState) {

		super.onCreate(savedInstanceState);

		//This EditText exists purely so the IME has somewhere to type; the app draws its own content.
		//TextView answers the framework with an I-beam for anything editable, so the pointer turns into a
		//text caret over the whole window even though there's no text to select.
		//setPointerIcon can't fix that: TextView returns the caret before ever consulting it.

		inputField = new EditText(this) {

			@Override
			public PointerIcon onResolvePointerIcon(MotionEvent event, int pointerIndex) {
				return PointerIcon.getSystemIcon(getContext(), PointerIcon.TYPE_ARROW);
			}
		};

		//Allow keyboard

		inputField.setFocusable(true);
		inputField.setFocusableInTouchMode(true);
		inputField.setOnTouchListener((v, event) -> true);
		inputField.setShowSoftInputOnFocus(true);
		
		inputField.addTextChangedListener(new TextWatcher() {

			@Override
			public void onTextChanged(CharSequence s, int start, int before, int count) {

				if(count == 0)
					return;

				onTypeChar(s.toString());
				inputField.setText("");		//Input field is virtual only (selection has to be handled manually)
			}

			@Override
			public void beforeTextChanged(CharSequence s, int start, int count, int after) { }

			@Override
			public void afterTextChanged(Editable s) { }
		});

		//Disallow other shenanigans such as autofill, cursor showing up
		//Also disallow toggling it by clicking the view.
		//The OxC3 app itself is responsible for bringing this up through toggleKeyboard (Platform_setKeyboardVisible)

		inputField.setClickable(false);
		inputField.setCursorVisible(false); 
		inputField.setLongClickable(false);
		inputField.setTextIsSelectable(false);

		setContentView(inputField);

		try {
			ActivityInfo activityInfo = getPackageManager().getActivityInfo(getComponentName(), PackageManager.GET_META_DATA);
			System.loadLibrary(activityInfo.metaData.getString("android.app.lib_name"));
		} catch(PackageManager.NameNotFoundException ex) {
			Log.e("OxC3", "Couldn't find lib_name in apk file");
		}
	}

	//Callbacks called from OxC3

	public int getDeviceOrientation() {
		return getWindowManager().getDefaultDisplay().getRotation() * 90;
	}

	//The NDK only has ANativeWindow_setFrameRate (a setter), so the monitor's refresh rate needs this

	public float getRefreshRate() {
		return getWindowManager().getDefaultDisplay().getRefreshRate();
	}

	//Localized label for a key, used by Keyboard_remap (option screens, "press a key" prompts).
	//The NDK has no equivalent; AKeyEvent_* only exposes the raw keycode, so this has to go through
	// the framework's KeyCharacterMap.
	//deviceId is the id of the keyboard that last sent a key event,
	// or KeyCharacterMap.VIRTUAL_KEYBOARD (-1) when nothing physical has been seen yet.
	//Returns "" when the key has no printable representation (modifiers, arrows, F-keys, ...).

	public String getKeyLabel(int keyCode, int deviceId) {

		try {

			KeyCharacterMap map = KeyCharacterMap.load(deviceId);

			if(map == null)
				return "";

			//What's physically printed on the key; this is what an option screen wants to show

			char label = map.getDisplayLabel(keyCode);

			if(label != 0)
				return String.valueOf(label);

			//Keys with no printed glyph can still produce a character; fall back to the unmodified one.
			//Dead keys report their accent with the COMBINING_ACCENT bit set, so mask it off.

			int unicode = map.get(keyCode, 0);

			if((unicode & KeyCharacterMap.COMBINING_ACCENT) != 0)
				unicode &= KeyCharacterMap.COMBINING_ACCENT_MASK;

			return unicode == 0 ? "" : new String(Character.toChars(unicode));
		}

		//load() throws UnavailableException if the id is unknown (keyboard unplugged between the
		//key event and the remap call). An empty label is the documented "no remap available".

		catch(Exception ex) {
			return "";
		}
	}

	//Character a key produces for the current layout and modifier state, for the onTypeChar callback.
	//Physical keys don't go through the EditText (it only has focus while the soft keyboard is up),
	// so they'd otherwise produce no text at all. 0 means the key produces nothing printable.

	public int getKeyUnicode(int keyCode, int metaState, int deviceId) {

		try {

			KeyCharacterMap map = KeyCharacterMap.load(deviceId);

			if(map == null)
				return 0;

			int unicode = map.get(keyCode, metaState);

			//Dead keys (accents) only resolve once combined with the next key, so there's nothing to type yet

			return (unicode & KeyCharacterMap.COMBINING_ACCENT) != 0 ? 0 : unicode;
		}

		catch(Exception ex) {
			return 0;
		}
	}

	public void toggleKeyboard(boolean show) {

		//Can't call this from any random thread, we have to ask the UI thread

		runOnUiThread(new Runnable() {
			@Override
			public void run() {   

				InputMethodManager imm = (InputMethodManager) inputField.getContext().getSystemService(Context.INPUT_METHOD_SERVICE);

				if (show) {
					inputField.requestFocus();
					imm.showSoftInput(inputField, InputMethodManager.SHOW_IMPLICIT);
				}
				
				else imm.hideSoftInputFromWindow(inputField.getApplicationWindowToken(), 0);	
			}
		});
	}
}