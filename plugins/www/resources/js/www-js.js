/*jshint esversion: 6 */
function setLoginFields() {
    var evt = new Event('change');

    var frames = window.frames;

    var doc;
    var pswdField;
    var usrField;
    var formNode;
    var userFound = false;

    if (frames.length != 0) {
        for (var i = 0; i < frames.length; i++) {
            doc = frames[i].document;
            pswdField = doc.querySelectorAll("input[type='password']");
            if ((pswdField !== undefined) && (pswdField !== null)) {
                break;
            }

        }
        if ((pswdField === undefined) || (pswdField === null)) {
            /* What if we don't have login forms in the iFrame? -> window */
            doc = window.document;
            pswdField = doc.querySelectorAll("input[type='password']");
        }
    } else {
        doc = window.document;
        pswdField = doc.querySelectorAll("input[type='password']");

    }


    if (pswdField !== undefined) {
        pswdField.forEach(function(pswdElement) {
            if (pswdElement.getAttribute('autocomplete') !== 'new-password') {
                pswdElement.value = 'PWDPLACEHOLDER';
            }

            formNode = pswdElement.form;
            if (formNode !== null) {
                console.debug("Form elements found");
                usrField = formNode.querySelectorAll("input[type='text']");

                usrField.forEach(function(usrElement) {
                    usrElement.value = 'USRPLACEHOLDER';
                    if (usrElement !== null) {
                        usrElement.dispatchEvent(evt);
                        userFound = true;
                    }
                });
                pswdElement.dispatchEvent(evt);
            }
            if (formNode === null || !userFound) {
                console.debug("Form elements found");
                console.debug("Inputs elements may be in other containers");
                var inputs = doc.getElementsByTagName('input');
                for (var i = 0; i < inputs.length; i += 1) {
                    console.debug("input type: " + inputs[i].type);
                    switch(inputs[i].type) {
                        case 'new-password':
                            continue;
                        case 'password':
                            continue;
                        case 'hidden':
                            continue;
                        case 'email':
                            inputs[i].value = 'USRPLACEHOLDER';
                            userFound = true;
                            break;
                        case 'text':
                            if(!userFound) {
                                inputs[i].value = 'USRPLACEHOLDER';
                                userFound = true;
                            }
                        default:
                            console.debug("Tentativily try to add the username if no userFound");
                            if(!userFound) {
                                inputs[i].value = 'USRPLACEHOLDER';
                                userFound = true;
                            }
                            // code block
                    }
                    if(userFound) {
                        inputs[i].dispatchEvent(evt);
                        console.debug("User name field found and set(?)");
                        break;
                    }
                }
            }
        });
    }
}

setLoginFields();
