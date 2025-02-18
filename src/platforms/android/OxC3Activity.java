/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
*  Copyright (C) 2023 - 2025 Oxsomi / Nielsbishere (Niels Brunekreef)
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

package net.osomi.nativeactivity;

import android.content.pm.ActivityInfo;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.app.NativeActivity;
import android.content.Context;
import android.util.Log;
import android.view.KeyEvent;
import android.view.inputmethod.InputMethodManager;
import android.widget.EditText;
import android.text.*;

public class OxC3Activity extends NativeActivity {

	private static EditText inputField;		//Show soft keyboard if requested from OxC3

	public native void onTypeChar(String input);

	@Override
	protected void onCreate(Bundle savedInstanceState) {

		super.onCreate(savedInstanceState);

		inputField = new EditText(this);

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