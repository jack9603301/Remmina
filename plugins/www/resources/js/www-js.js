
/* Some JS examples */


// Get all frames and do something with each

for (i = 0; i < window.frames.length; i++) {
	// now window.frames[i] would be the current frame in the loop
	// var inputs = window.frames[i].contentDocument.getElementByTagName('input')
}



// Get user/password than...
function getLoginFields() {
	var fieldPairs = [],
		pswd = (function(){
			var inputs = document.getElementsByTagName('input'),
				len = inputs.length,
				ret = [];
			while (len--) {
				if (inputs[len].type === 'password') {
					ret[ret.length] = inputs[len];
				}
			}
			return ret;
		})(),
		pswdLength = pswd.length,
		parentForm = function(elem) {
			while (elem.parentNode) {
				if(elem.parentNode.nodeName.toLowerCase() === 'form') {
					return elem.parentNode;
				}
				elem = elem.parentNode;
			}
		};
	while (pswdLength--) {
		var curPswdField = pswd[pswdLength],
			parentForm = parentForm(curPswdField),
			curField = curPswdField;
		if (parentForm) {
			var inputs = parentForm.getElementsByTagName('input');
			for (var i = 0; i < inputs.length; i++) {
				if (inputs[i] !== curPswdField && inputs[i].type === 'text') {
					fieldPairs[fieldPairs.length] = [inputs[i], curPswdField];
					break;
				}
			}
		}
	}
	return fieldPairs;
}

// Do something with what we have found
var loginFields = getLoginFields()[0]; // or loop through results.


// Firefox, sice Feb 2019 does not fill inputtype=password and autocomplete="new-pèassword"
if (!userTriggered && passwordField.getAutocompleteInfo().fieldName == "new-password") {
	log("not filling form, password field has the autocomplete new-password value");
	autofillResult = AUTOFILL_RESULT.PASSWORD_AUTOCOMPLETE_NEW_PASSWORD;
	return;
}
