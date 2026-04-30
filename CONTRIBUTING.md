## Contributing to Gnome Commander

Thank you for your interest in helping Gnome Commander development! There is a number of ways for you to help. But one note first:

⚠ IMPORTANT! This project accepts **only** contributions that are 100% human-made. Please do not contribute text, translations, images or code which have been in part or fully produced by LLMs (so-called “AI”). If you need an explanation you can find one [at the bottom of this text](#why-we-dont-want-ai-contributions).

### Reporting issues

Believe it or not: complaining is useful. Gnome Commander shortcomings that may be obvious to you are not necessarily obvious to other people. Everybody has their own workflows, and because of that we all see only one part of the picture. Contribute yours!

It doesn’t have to be a bug report, ideas that would make your life easier are just as welcome. Even if we don’t accept your idea as it is, it provides valuable information about the expectations people have towards Gnome Commander. And who knows, we might come up with something even better.

That said, there are ways to make your contribution more useful:

* Always run the lastest version of Gnome Commander. If you can: [build the development version](INSTALL) and test it as well. Maybe the issue is resolved already.
* Search existing reports first, maybe somebody reported this issue already. Please refrain from commenting in resolved reports however, usually your issue will be similar but different. You can add a link to that report to yours.
* Please provide all relevant context information: your operating system, desktop environment, Gnome Commander version, any configuration changes which might be related, console output showing up if you run Gnome Commander from command line etc.
* Describe the issue in terms that allow us to reproduce it: what were your exact steps that led to the issue? What happened then and what did you expect to happen? Does it happen every time? If not, do you have an idea when it happens? You can also record a short screen video if you are unsure how to describe the issue.

You can see the existing issues and report new ones [on GitLab](https://gitlab.gnome.org/GNOME/gnome-commander/-/work_items).

### Translating

Gnome Commander is available in a number of languages, and these translations are maintained by volunteers. If you want to help with that effort, feel free to register on [Damned Lies](https://l10n.gnome.org/module/gnome-commander/), GNOME’s translation platform.

Please keep in mind: translating isn’t always straightforward. You translate the *meaning* of the text, not the words. Sometimes this means using a formulation that is very different from the original because it works better in your language. Context also matters, same text might mean different things in different contexts. With user interfaces shorter is generally better even if not quite as precise.

### Contributing code

See the [INSTALL file](INSTALL) for instructions on building and running the code. If you want to contribute changes back you will need to create a merge request. Here is how it works:

* Fork this repository on GitLab if you haven’t done so yet. You can then click the Code dropdown and get the address that you need to clone:
  ```bash
  git clone https://gitlab.gnome.org/<user>/gnome-commander.git
  cd gnome-commander
  ```
* Set up the upstream repository, allowing you to sync your fork with it easily:
  ```bash
  git remote add upstream https://gitlab.gnome.org/GNOME/gnome-commander.git
  ```
* Create a branch for your work:
  ```bash
  git checkout -b my-awsome-feature
  ```
* Make and test your changes. Once you are done commit them with `git commit`. If your work addresses an open issue, say #123, add something like “Closes: #123” to your commit message. This will make sure the issue is closed automatically when your changes are merged.
* Just to be on the safe side, synchronize with the upstream repository and rebase your changes on top of the latest `main` branch state. This will make sure that there are no merge conflicts:
  ```bash
  git fetch upstream main
  git rebase upstream/main
  ```
* Push your changes to your fork on GitLab:
  ```bash
  git push origin HEAD
  ```
* Create your merge request. Either use the link shown to you on the terminal when you push or open the branch you created for these changes on GitLab and click “Create merge request.”

Now it is up to us. We will try to review your merge request timely and either request changes or merge it.

A word of caution: if you plan to make extensive changes then we should hear about it *before* you’ve done all the work. Feel free to create an issue where conceptual questions can be discussed. Creating a draft merge request with your work in progress isn’t a bad idea either, it lets us comment on the direction you are taking. You don’t want to have all your work go to waste because we have different ideas on how something should be done.

#### Tip for working with git ###

This repository contains a `git-scripts` directory with a [pre-commit](git-scripts/pre-commit) and a [pre-push](git-scripts/pre-push) hook. Type `ln -s git-scripts/pre-commit .git/hooks/pre-commit` and similarly for the pre-push hook to
activate each hook.

Both scripts run simple checks before actually committing or pushing
your source code changes.

#### Branches and Versions

Gnome Commander is being developed in several branches:

* The main branch where all the normal development takes place. It should always contain a runnable version of Gnome Commander.
* One or more stable branches, named after the current stable release (e.g. gcmd-1-18), which only include bugfixes, doc updates, translation updates, but no new features.
* Optionally, some feature branches where new ideas or big features are cooked until they are ready to be merged into the main branch.

The version numbers (major, minor and micro) follow the standard of odd and even versioning. Even numbers are stable versions that are intended for all-day use. Odd versions are development versions. The current version number is stored at the top of [meson.build](meson.build).

### Why we don’t want “AI” contributions

Looking purely from the technical point of view the current generation of LLMs has some valid use cases. Gnome Commander isn’t one of them:

* Maintaining code requires a deep understanding of it. Because of that we depend on contributors who understand their contributions and the decisions that went into them. Even if you don’t plan on contributing again, you can answer questions about your contribution and share your thought process – an LLM can only fake it.
* LLMs have the tendency of producing plausible looking and on the first glance maybe even working but still wrong/suboptimal solutions. Submitting these is a waste of everybody’s time.
* By making LLMs do the work for you you deny yourself an opportunity for growth. Open Source is shorthanded as it is, and people never improving their coding skills because they rely on LLMs instead is going to make the situation much worse (deskilling).
* Writing, drawing, coding is creative work. Outsourcing creative tasks to computers and only leaving the boring “manual” work to humans would be a shame. This is not how it is supposed to work. And that’s not even considering the fact that these computers tend to remix existing information without ever coming up with anything new.

Given the technical points you may be inclined to use LLMs “only” to assist – what would be the harm in that? Studies show that people using LLMs for assistance grow increasingly reliant on them and stop questioning the results. But even if you trust yourself not to fall for this trap, there are also massive ethical issues that make us refuse LLM use altogether:

* LLMs are being used as a way of profiting from other people’s work without crediting them. In particular, LLMs are being trained on code from open source projects and will produce variations of this code ignoring licenses of the originals.
* The environmental impact of LLMs (e.g. due to power and water requirement) stands in no proportion to their usefulness. LLMs are quickly becoming a significant factor in making our planet unsuitable for life. Never mind them causing resource shortages that affect all of us.
* LLMs concentrate power in the hands of selected few. They are already being abused to rewrite history and facts to better suit an agenda. This stands diametrically opposite to the open source movement’s goals.
