
/* Some JS examples */



// Get user/password input fields than...
function getLoginFields(frame) {
    var fieldPairs = [],
        pswd = (function(){
            var inputs;
            var len;
            var ret;
            if ((frame !== undefined) && (frame !== null)) {
                inputs = document.getElementsByTagName('input');
            } else {
                inputs = frame.contentDocument.getElementByTagName('input');
            }
            len = inputs.length;
            ret = [];
            while (len--) {
                if (inputs[len].type === 'password'
                    && inputs[len].getAttribute('autocomplete') !== 'new-password') {
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


/*
// Firefox, sice Feb 2019 does not fill inputtype=password and autocomplete="new-pèassword"
if (!userTriggered && passwordField.getAutocompleteInfo().fieldName == "new-password") {
    log("not filling form, password field has the autocomplete new-password value");
    autofillResult = AUTOFILL_RESULT.PASSWORD_AUTOCOMPLETE_NEW_PASSWORD;
    //return;
}
*/
function setLoginData(frame, loginField) {
    let evt = new Event('change');
    elementLoginId = loginField.id;
    elementLoginType = loginField.attr('type');
    if ((frame !== undefined) && (frame !== null)) {
        if ((elementLoginType !== undefined) && (elementLoginType === 'password')) {
            document.getElementById(frame).contentDocument.getElementById(elementLoginId).value = 'PWDPLACEHOLDER';
        } else {
            document.getElementById(frame).contentDocument.getElementById(elementLoginId).value = 'USRPLACEHOLDER';
        }
        document.getElementById(frame).contentDocument.getElementById(elementLoginId).dispatchEvent(evt);
        document.getElementById(frame).contentDocument.getElementById(elementLoginId).dispatchEvent(evt);
    } else {
        if ((elementLoginType !== undefined) && (elementLoginType === 'password')) {
            document.getElementById(elementLoginId).value = 'PWDPLACEHOLDER';
        } else {
            document.getElementById(elementLoginId).value = 'USRPLACEHOLDER';
        }
        document.getElementById(elementLoginId).dispatchEvent(evt);
        document.getElementById(elementLoginId).dispatchEvent(evt);

}

var frames = window.frames;
var i;
var loginFields;

if (frames.length != 0) {
    for (i = 0; i < frames.length; i++) {
        //loginFields = getLoginFields(frames[i])[0]; // or loop through results.
        // we loop through something like:
        // [ [ <input id="username" /> , <input type="password" id="pswd" /> ] ]
        for (c = 0; c < getLoginFields(frames[i]).length; c++) {
            // Do something with each login field
        }
    }
} else {
    //loginFields = getLoginFields(null)[0]; // or loop through results.
    for (c = 0; c < getLoginFields(null).length; c++) {
        // Do something with each login field
    }
}
