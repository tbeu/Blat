INDEX

- Description
- Installation
- What's New              (See ChangeLog.txt)
- Usage                   (run 'Blat /?')
- Examples                (See www.blat.net)
- Copyright               (See license.txt)
- Authors & Contributors  (See credits.html)


DESCRIPTION:

You can find Blat at http://www.blat.net

Blat is a Public Domain (generous aren't we?) Win32/Win64 console utility
that sends the contents of a file in an e-mail message using the SMTP
protocol. Blat is useful for creating scripts where mail has to be sent
automatically (CGI, backups, etc.) To use Blat you must have access to a
SMTP server via TCP-IP. Blat can store various default settings in the
registry. The command line options override the registry settings. Input
from the console (STDIN) can be used instead of a disk file (if the
special filename '-' is specified). Blat can also "carbon copy" and "blind
carbon copy" the message. Impersonation can be done with the -i flag which
puts the value specified in the "From:" line, however when this is done
the real senders address is stamped in the "Reply-To:" and "Sender:"
lines. This feature can be useful when using the program to send messages
from users that are not registered on the SMTP host.

Optionally, Blat can also attach multiple files to your message.


INSTALLATION:

If you are upgrading from version 1.2 or later, simply copy Blat.exe
over the old one. Blat no longer needs gensock.dll or gwinsock.dll. You
can delete these unless another application you use requires them.

If you are upgrading from Blat 1.1 or 1.0 (phew!) or you never used Blat
before you can follow these steps:

1) Copy the file "Blat.exe" to your "\WINNT\SYSTEM32" directory, or to
   any other directory in your path.

2) Optionaly, run "Blat -install smtp.yoursite.tld youruserid@yoursite.tld"

