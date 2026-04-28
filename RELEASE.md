# Releasing

This file describes how to perform a new release of Gnome Commander using the GNOME infrastructure.

Last update: 2026-04-28

## Preparing a Release

General hints about doing a release can be found in the GNOME release guidelines:
https://handbook.gnome.org/maintainers/making-a-release.html


## Branching before the next stable release

If a new stable release should be prepared, a new branch must be created some time before the release. The naming scheme should be `gcmd-X-Y`, where `X` is the major version and `Y` the minor version of the program.

### Inform the translators about the new branch

After creating that new stable branch, either the GNOME translators need to be informed about that branch by opening a new issue in "Damned Lies" similar to how it was done [here](https://gitlab.gnome.org/Infrastructure/damned-lies/-/issues/354). Also, the previous stable branch can be mentioned in that issue to not be supported anymore.

Another option is: If you have write access to the Gnome Commander project on [Damned Lies](https://l10n.gnome.org/module/gnome-commander/), you can configure the branches in Damned Lies by yourself. Usually, the person which is mentioned in the [gnome-commander.doap](gnome-commander.doap) file is eligible for doing that. If in doubt how to login, use the "Legacy Connection" method to login, and use the email mentioned in the doap-file. You will recieve an email with a login link. (At the very first time this did not work for me and I needed some [help](https://gitlab.gnome.org/Infrastructure/damned-lies/-/work_items/532#note_2159747) as some changes on the backend were missing.)

## Branching before the next bug fix release

Bug fix releases should only happen from inside an already existing stable branch. Hence, no new branch needs to be created. Also, the translation team does not need to be informed if no massive changes in translatable strings occurred. This is usually the case for bug fix releases.

## Days before the release

Update the [MetaInfo](data/org.gnome.gnome-commander.metainfo.xml.in) file with the new features and other changes of the upcoming release.

## On the day of the release

When preparing the release, update the release date in the [MetaInfo](data/org.gnome.gnome-commander.metainfo.xml.in) file and also in the [documentation](doc/C/index.docbook) in the header and in release notes (similar to how it was done [here](https://gitlab.gnome.org/GNOME/gnome-commander/-/commit/5b543557c790b5be6d70bb56ad508727a2754d37)).

Then, do a `appstream-util validate data/org.gnome.gnome-commander.metainfo.xml.in`. No error should occur.

Take note of https://handbook.gnome.org/maintainers/making-a-release.html on the release day. Ignore what is mentioned there about the NEWS file.

By tagging the release (previously we always used `git tag -s <tag>` for that) and pushing it to GNOME GitLab, the commands in the `create_tarball` section of the file [.gitlab-ci.yml](.gitlab-ci.yml) will be executed on GitLab and a tar.xz archive will be created on the [GNOME download server](https://download.gnome.org/sources/gnome-commander/). A new release version should also occur in the [GitLab release section](https://gitlab.gnome.org/GNOME/gnome-commander/-/releases). The maintainer should copy-paste the changes of the current release from the MetaInfo file to the new release entry there and also link the new tar.xz-Archive from the Download server.

Finally, do a post-release version bump in the current stable branch like [this one](https://gitlab.gnome.org/GNOME/gnome-commander/-/commit/c9b0fc6a2be2e7df9644cd2730a069103c4a6a98). For this, increase the release version and put the next release date on a day in the future. That's it.