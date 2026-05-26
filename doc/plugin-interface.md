# The Gnome Commander plugin interface

Gnome Commander allows delegating some of the work to plugins which are essentially external applications. These applications can be written in any programming language and use the [native messaging protocol](https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/Native_messaging#app_side) to exchange JSON messages with Gnome Commander.

## Installing plugins

Gnome Commander plugins can be installed system wide or for a particular user account. The directory for system plugins is typically `/usr/lib64/gnome-commander/plugins` but can vary if the Gnome Commander build was configured with a different library directory. The user plugin directory is usually `~/.config/gnome-commander/plugins`. If a plugin with the same file name is present in both directories, the per-user installation takes precedence.

Plugins should be installed at the top level of the plugins directory, any plugins nested within subdirectories will not be considered. A plugin can be either a single executable file or a directory. If it is the latter, the directory is expected to contain an executable file `main` which will be started.

## The Python plugin_helper module

Gnome Commander provides the `plugin_helper` module which can be imported by Python-based plugins. Its goal is simplifying plugin development by removing the boilerplate code. The simplest plugin then looks like this:

```python
#!/usr/bin/env python3
from plugin_helper import Plugin

class MyPlugin(Plugin):
    async def startup(self):
        self.send_message('info', {
            'name': 'My Plugin',
            'version': '1.0',
        })
        self.send_message('ready')

if __name__ == '__main__':
    MyPlugin().run_forever()
```

The [test plugin](../plugins/test.py) provides a more elaborate example.

While the `plugin_helper` module certainly simplifies things, it has two important caveats:

* It is available for Python only. Plugins written in other programming languages can use the message exchange protocol directly or develop their own equivalent.
* It is tied to the installed Gnome Commander version, meaning that its API could change. Unlike the messaging protocol it has no mechanism to negotiate capabilities. So if an incompatible `plugin_helper` module is a concern for you the easiest solution should be packaging the current version of the module with your plugin instead of using the provided one.

## Environment variables

Gnome Commander sets the following environment variables when starting plugins:

* `PYTHONPATH`: Points to the system plugins directory, allows per-user plugins written im Python to import `plugin_helper` module.
* `GETTEXT_DOMAIN`: Should have the value `gnome-commander`, the gettext domain to be used by plugins using Gnome Commander’s translations.
* `GETTEXT_DIR`: Should have a value like `/usr/share/locale`, the directory containing gettext locales used by Gnome Commander.

## The communication protocol

The plugin interface uses the [native messaging protocol](https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/Native_messaging#app_side), meaning that plugin receive messages from Gnome Commander as JSON data on stdin and send their own JSON-encoded messages to stdout. The messages are described below.

*Note*: A plugin can print messages to stderr for debugging. These will show up in Gnome Commander’s stderr stream, e.g. if Gnome Commander is started from console.

All messages are JSON objects with two fields:

* `type` (string): Type of the message, e.g. `"ready"`.
* `payload`: Type-dependent data. This can be `null` for messages that don’t carry any data.

Note that sending messages that Gnome Commander cannot recognize will result in the plugin being disabled. Similarly, plugins that fail to initialize within 10 seconds will be disabled. This guards against using applications as plugins which are not meant for it.

### General messages

The general messages are available regardless of which APIs the plugin registered for. Most of them are used in the startup phase of the plugin.

#### apis

A message of this type is always sent by Gnome Commander when a plugin starts up. Its payload is an array of plugin APIs supported by this Gnome Commander version, for example:

```json
{
  "type": "apis",
  "payload": [
    {"name": "extract_metadata", "version": "1.0"},
    ...
  ]
}
```

#### register

This message should be sent by the plugin during the initialization phase to indicate that it intends to use a particular API. The payload it the API (name and version) to register for, for example:

```json
{
  "type": "register",
  "payload": {"name": "extract_metadata", "version": "1.0"}
}
```

This will make the plugin receive messages whenever Gnome Commander needs to extract metadata from a file. This message can be sent multiple times, registering for different APIs.

#### info

This message should be sent by the plugin during the initialization phase to set the information which should be displayed for it in the Plugins dialog. The payload is an object containing the following values:

* `name` (string): Name of the plugin
* `version` (string): Plugin version number
* `copyright` (string, optional): Plugin’s copyright message
* `comments` (string, optional): A longer description of the plugin
* `authors` (array, optional): List of plugin authors
* `documenters` (array, optional): List of people contributing to plugin’s documentation
* `translators` (array, optional): List of people contributing to plugin’s translations
* `webpage` (string, optional): Address of the plugin’s webpage

#### ready

Sent by the plugin to end the initialization phase, indicating that the plugin is ready. It will receive messages specific to the registered APIs after this message. This message has no payload.

#### failed

Sent by the plugin to end the initialization phase and disable the plugin. This message will typically be sent in case of a missing dependency. The plugin is expected to send one or multiple `error` messages to explain the reason for the failure. This message has no payload.

#### error

This message is sent by the plugin to report an error that should be displayed to the user. The payload of this message is a string containing the error message.

#### api-request

This message contains an API request sent from Gnome Commander to the plugin or vice versa. The payload is an array with two elements: `[id, request]`. Here `id` is an integer which should be sent along with the response if a response is expected. `request` is an object with a single property. The name of that property indicating the type of the request whereas its value contains the request data, for example:

```json
{
  "type": "api-request",
  "payload": [
    12,
    {"get-setting": "config"}
  ]
}
```

Request types and the corresponding data are API-specific and will be explained below.

#### api-response

This message is sent in response to a preceding `api-request` message and is structured in the same way. Its payload is an array with two elements: `[id, request]`. Here `id` is the identifier of the request being replied to and `request` is an object with a single property. The name of this property should match the request type from the API request and its value should contain the response data (which could be `null`). For example, the `get-setting` request above could be answered with:

```json
{
  "type": "api-response",
  "payload": [
    12,
    {"get-setting": {
      "a": 40,
      "b": null,
      "c": "anything"
    }}
  ]
}
```

Response data depends on request type and is API-specific.

**Important**: A plugin should *always* send a response to API requests if a response is expected. Otherwise Gnome Commander will wait 5 seconds for a response from a plugin before reporting an error.

### Persistent settings API

Persistent settings API allows plugins to save arbitrary configuration data. Note that this API is not meant for large quantities of data. Plugins that need to store a significant amount of data should create a configuration file.

Using this API requires sending a registration message:

```json
{
  "type": "register",
  "payload": {"name": "persistent_settings", "version": "1.0"}
}
```

#### get-setting

The `get-setting` API request is sent by the plugin to retrieve the value of a setting. The request data is a string indicating the setting name. The response contains the setting value (arbitrary JSON data) or `null` if the setting hasn’t been set yet.

#### set-setting

The `set-setting` API request is sent by the plugin to store a setting value. The request data is an array with two elements: `[setting, value]`. Here `setting` is a string indicating the setting name and `value` is an arbitrary JSON value to be stored. This request has no response.

### Metadata extraction API

Metadata extraction API will query the plugin whenever Gnome Commander needs to display metadata for a file, e.g. in the internal viewer or File Properties dialog. Using this API requires sending a registration message:

```json
{
  "type": "register",
  "payload": {"name": "extract_metadata", "version": "1.0"}
}
```

#### extract-metadata

Gnome Commander will send this API request when it needs file metadata to be extracted. The request data is an object with two properties:

* `path` (string): Local file path if any
* `uri` (string): File URI

The response data should be an array of tag groups which can be empty if the plugin cannot extract data from this file. Each tag group is an object with two fields:

* `class` (string): A group name, not necessarily unique and possibly localized
* `tags` (array): A list of tags

Each tag is an array with four strings: `[id, name, description, value]`. Here `id` is a tag identifier which isn’t displayed to the user and should not be localized. `name` is a short tag name which can be localized and should not be empty. `description` is an extended tag description which can be localized and can be left empty. Finally `value` is the tag value extracted from the file.

#### list-supported-tags

Gnome Commander will send this API request when it needs to display a grouped list of supported tags, e.g. in the Advanced Rename Tool. The request has no data. Note that a plugin doesn’t necessarily have to return all tags possibly produced in response to an `extract-metadata` request but it is desirable to list at least all the typical tags here.

The response data should be an array of tag groups. Each tag group is an object with two fields:

* `class` (string): A group name, not necessarily unique and possibly localized
* `tags` (array): A list of supported tags

The tags are array similar to those in the `extract-metadata` response but only contain two string: `[id, name]`. Here `id` is a tag identifier which is not localized and matches the tag ID produced by `extract-metadata`. `name` is a short tag name which will be displayed to the user and can be localized.

### Menus API

The menus API allows plugins to add their menu items to Gnome Commander’s main menu and the file context menu. Using this API requires sending a registration message:

```json
{
  "type": "register",
  "payload": {"name": "menus", "version": "1.0"}
}
```

#### main-menu-items

This request will be sent to the plugin to retrieve the menu items to be added to the main menu. These will be displayed in the Plugins submenu. This request has no data. Note that this request will typically be sent multiple times over the lifecycle of a plugin but the menu items are expected to stay unchanged.

The response data should be an array of menu items which can be empty if the plugin doesn’t have any menu items to be display in the main menu. A menu item is an object with the following properties:

* `label` (string): Text to be displayed to the user, possibly localized
* `action` (string): An action identifier, allowing the plugin to recognize which menu item was activated
* `parameter` (string, optional): An additional parameter to be sent when the menu item is activated

#### context-menu-items

This request will be sent to the plugin whenever the file context menu is displayed. The request data is an object indicating current Gnome Commander state. The properties of the state object are:

* `active_directory_path` (string or `null`): Path of the directory displayed in the active panel
* `active_directory_uri` (string or `null`): URI of the directory displayed in the active panel
* `active_focused_file` (string or `null`): Name of the focused file in the active panel if any
* `active_selected_files` (array of string): Names of the selected files in the active panel, can be empty
* `inactive_directory_path` (string or `null`): Path of the directory displayed in the inactive panel
* `inactive_directory_uri` (string or `null`): URI of the directory displayed in the inactive panel
* `inactive_focused_file` (string or `null`): Name of the focused file in the inactive panel if any
* `inactive_selected_files` (array of string): Names of the selected files in the inactive panel, can be empty

The response data should be an array of menu items which can be empty if the plugin doesn’t have any action which apply to the current Gnome Commander state. The menu item objects are the sames as in responses to the [main-menu-items request](#main-menu-items).

#### menu-activated

This request will be sent to the plugin whenever one of its menu items is activated. The request data is an object with the following properties:

* `action` (string): Action of the menu item that was activated
* `state` (object): Current Gnome Commander state, same as the object sent in the [context-menu-items request](#context-menu-items)
* `parameter` (string): The additional parameter associated with the menu item if any

Note that plugins should not assume that the state didn’t change between displaying the context menu and menu activation. If current state is relevant, the state provided with this message should be considered.

This request has no response.

### Dialogs API

This API allows plugins to display simple dialogs to the user, showing messages and collecting user input. It does not provide a means to react to user input before the dialog is dismissed however.

Using this API requires sending a registration message:

```json
{
  "type": "register",
  "payload": {"name": "dialogs", "version": "1.0"}
}
```

#### show-dialog

The plugin sends this request to have a dialog shown to the user. The request data is an object with the following properties:

* `title` (string): The dialog title
* `modal` (boolean, optional): Whether the dialog should be modal (default is `false`)
* `width` (integer, optional): Set the default width for the dialog (default is 600)
* `child` (WidgetSpec object): The widget to be displayed insider the dialog
* `buttons` (array of objects): The dialog buttons as objects with the following properties:
  * `id` (string): The button identifier to be used in the response
  * `label` (string): The user-visible and possibly localized button label. Use underscore _ to indicate mnemonic character, e.g. `"_Cancel"` will allow using Alt+C key combination to trigger this button quickly.
  * `default` (bool, optional): Marks this button as the default action
  * `cancel` (bool, optional): Marks this button as the cancel action, to be triggered if Escape is pressed or the dialog’s close button is clicked. It is required that one of the dialog buttons is the cancel button.

The `WidgetSpec` object can be one of the following:
* A horizontal box, indicated by `"type": "HBox"` property. Additional properties:
  * `children` (array of WidgetSpec objects): Widgets to be arranged horizontally
* A vertical box, indicated by `"type": "VBox"` property. Additional properties:
  * `children` (array of WidgetSpec objects): Widgets to be arranged vertically
* Static text, indicated by `"type": "Text"` property. Additional properties:
  * `label` (string): The text to be displayed
* A text input field, indicated by `"type": "Input"` property. Additional properties:
  * `id` (string): The field identifier to be used in the response
  * `label` (string): Label to be associated with the field. Use underscore _ to indicate mnemonic character, e.g. `"_Text"` will allow using Alt+T key combination to jump to this field quickly.
  * `value` (string, optional): Initial input field text
  * `placeholder` (string, optional): Placeholder text to display when the input field is empty
  * `vertical` (boolean, optional): Whether the label and the input field should be arranged vertically instead of the default horizontal arrangement
* A widget group, indicated by `"type": "Group"` property. This widget not only groups related controls visibly but also improves accessibility by indicating that relationship to screen readers. Additional properties:
  * `label` (string): The group label to be displayed to the user
  * `child` (WidgetSpec object): The widget to be displayed within the group
* A checkbox, indicated by `"type": "Checkbox"` property. Additional properties:
  * `id` (string): The field identifier to be used in the response
  * `label` (string): Checkbox label. Use underscore _ to indicate mnemonic character, e.g. `"_Overwrite automatically"` will allow using Alt+O key combination to toggle this checkbox quickly.
  * `checked` (boolean, optional): Initial checkbox state
* A group of radio widgets, indicated by `"type": "RadioGroup"`. Any checkbox widgets within this group turn into radio widgets, so that only one of them can be checked at a time. Additional properties:
  * `children` (array of WidgetSpec objects): The widgets to be displayed within the group
  * `vertical` (boolean, optional): Whether the child widgets should be arranged vertically instead of the default horizontal arrangement

The response is sent to the plugin after the user clicks one of the dialog buttons or closes the dialog. The response data is an array with two elements: `[button, fields]`. Here `button` is a string indicating which button was chosen by the user, it will be the identifier of the cancel button if the dialog was closed. `fields` is an object capturing the state of the input fields and checkboxes, containing properties with property key being the field identifier and property value the field text (string) or checked state (boolean). It is up to the plugin to validate the information provided and possibly to send another `show-dialog` request if corrections have to be made.
