//
// Created by dc on 11/17/17.
//
#include <sys/mman.h>

#include <suil/email.h>
#include <suil/file.h>
#include <suil/base64.h>


namespace suil {

#define CRLF "\r\n"

    bool Email::Attachment::load() {
        if (data != nullptr) {
            // already loaded
            swarn("attachment already loaded");
            return true;
        }

        File reader(fname.data(), O_RDONLY, 0777);
        len = utils::fs::size(fname.data());
        if (len > 8180) {
            // used mapped buffer
            size = (size_t) len;
            int page_sz = getpagesize();
            size += page_sz - (size % page_sz);
            data = (char *) mmap(nullptr, (size_t) size, PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, -1, 0);
            if (data == MAP_FAILED) {
                swarn("mapping memory of size %lu for attchment failed: %s",
                      size, errno_s);
                return false;
            }
            mapped = true;
        } else {
            // allocated memory
            size = len + (len % 8);
            data = (char *) malloc((size_t) size);
            mapped = false;
        }

        size_t tread = 0, toread = 0;
        do {
            toread = MIN(8912, (len - tread));
            if (!reader.read(&data[tread], toread, 1500)) {
                return false;
            }
            tread += toread;
        } while (tread < len);

        return true;
    }

    void Email::Attachment::send(SocketAdaptor &sock, const String &boundary, int64_t timeout) {
        if (data == nullptr && !load()) {
            // could not load attachment
            throw Exception::create("Loading attachment failed");
        }

        size_t twrite = 0, written = 0, towrite = 0;
        do {
            towrite = MIN(8912, (len - twrite));

            written = sock.send(&data[twrite], towrite, timeout);
            if (written == 0) {
                throw Exception::create("sending attachment '", fname.data(), "' failed: ", errno_s);
            }
            twrite += written;
        } while (twrite < len);
    }

    Email::Attachment::~Attachment() {
        if (data != nullptr) {
            if (mapped) {
                munmap(data, (size_t) size);
            } else {
                free(data);
            }
            data = nullptr;
        }
        size = len = 0;
        mapped = false;
    }

    bool __internal::client::login(const String &domain, const String &username, const String &passwd) {
        int code{0};
        bool status{false};
        String user_b64(utils::base64::encode(username));
        String passwd_b64(utils::base64::encode(passwd));

        // get the server line
        if ((code = getresponse(5000)) != 220) {
            ierror("stmp connect error: %d - %s", showerror(code));
            goto client_login_do_quit;
        }

        // Send HELO
        if (!sendline(5000, "HELO ", domain)) goto client_login_exit;

        if ((code = getresponse(5000)) != 250) {
            ierror("stmp HELO error: %d - %s"), showerror(code);
            goto client_login_do_quit;
        }

        // Send login Request AUTH LOGIN
        if (!sendline(5000, "AUTH LOGIN")) goto client_login_exit;

        code = getresponse(5000);
        if (code != 334) {
            ierror("stmp AUTH LOGIN error: %d - %s\n", code, showerror(code));
            goto client_login_do_quit;
        }

        // send base64 encoded username and password
        if (!sendline(5000, user_b64)) goto client_login_exit;
        code = getresponse(5000);
        if (code != 334) {
            // sending email username failed
            ierror("stmp AUTH username rejected: %d %s", code, showerror(code));
            goto client_login_do_quit;
        }
        if (!sendline(5000, passwd_b64)) goto client_login_exit;
        code = getresponse(5000);
        if (code != 235){
            // login failure
            ierror("stmp AUTH password rejected: %d %s", code, showerror(500));
        } else {
            // logged in to server
            idebug("Successfully logged in to server");
            status = true;
            goto client_login_exit;
        }

    client_login_do_quit:
        quit();

    client_login_exit:
        return status;
    }

    int __internal::client::getresponse(int64_t timeout) {
        int code = -1;
        OBuffer b(127);
        size_t size = b.capacity();

        if (sock.read(&b[0], size, timeout)) {
            // Response successfully read, parse Response
            b.seek(size);
            String resp(b);
            String tmp(String(resp.data(), 3, false).dup());
            utils::cast(tmp, code);
            if (resp.size() > 6) {
                resp.data()[resp.size() - 2] = '\0';
                idebug("smtp_resp: %s", resp.data());
            }
            else {
                idebug("smtp_resp: %d", code);
            }
        } else {
            ierror("failed to receive Response from server");
        }

        return code;
    }

    void __internal::client::quit() {
        if (sock.isopen()) {
            if (sendline(5000, "QUIT")) {
                // send quit command
                char buf[32];
                size_t size = sizeof(buf);
                sock.read(buf, size, 5000);
            }
            // close underlying socket
            sock.close();
        }
    }

    const char* __internal::client::showerror(int code) {
        const char *error = "";
        switch (code) {
            case 252:
                error = "Cannot verify user, but will accept message and attempt delivery";
                break;
            case 421:
                error = "Service not available, closing transmission channel";
                break;
            case 450:
                error = "Requested mail action not taken: mailbox unavailable";
                break;
            case 451:
                error = "Requested action aborted: local error in processing";
                break;
            case 452:
                error = "Requested action not taken: insufficient system storage";
                break;
            case 500:
                error = "Syntax error, command unrecognised";
                break;
            case 501:
                error = "Syntax error in parameters or arguments";
                break;
            case 502:
                error = "Command not implemented";
                break;
            case 503:
                error = "Bad sequence of commands";
                break;
            case 504:
                error = "Command parameter not implemented";
                break;
            case 521:
                error = "Domain does not accept mail (see rfc1846)";
                break;
            case 530:
                error = "Access denied";
                break;
            case 550:
                error = "Requested action not taken: mailbox unavailable";
                break;
            case 552:
                error = "Requested mail action aborted: exceeded storage allocation";
                break;
            case 553:
                error = "Requested action not taken: mailbox name not allowed";
                break;
            case 554:
                error = "Transaction failed";
                break;
            default:
                error = "Unknown/Unhandled error code";
                break;
        }

        return error;
    }

    String Email::gethead(const Address &from) {
        OBuffer b(127);
        b << "From: ";
        from.encode(b);
        b << CRLF;
        b << "To: ";
        bool first{true};
        for (auto &addr : receipts) {
            if (!first) b << ", ";
            else first = false;
            addr.encode(b);
        }
        b << CRLF;

        if (!ccs.empty()) {
            b << "Cc: ";
            first = true;
            for (auto &addr : ccs) {
                if (!first) b << ", ";
                else first = false;
                addr.encode(b);
            }
            b << CRLF;
        }

        // Append subject
        b << "Subject: " << subject << CRLF;
        b << "MIME-Version: 1.0" CRLF;
        b << "Date: " << Datetime()(Datetime::HTTP_FMT) << CRLF;

        if (attachments.empty()) {
            b << "Content-Type: " << body_type << "; charset=utf-8" CRLF;
            b << "Content-Transfer-Encoding: 8bit" CRLF CRLF;
        } else {
            b << "Content-Type: multipart/mixed; boundary=\""
              << boundry << "\"" CRLF CRLF;
        }

        return std::move(String(b));
    }

    bool __internal::client::sendaddresses(const std::vector<Email::Address> &addrs, int64_t timeout) {
        int code = 0;
        for (auto &addr : addrs) {
            if (!sendline(timeout, "RCPT TO: <", addr.email, ">")) {
                ierror("sending messaged failed 'RCPT TO: %s'", addr.email.data());
                return false;
            }

            code = getresponse(timeout);
            if (code != 250) {
                ierror("stmp RCPT TO: <%s> failed: %s", addr.email.data(), showerror(code));
            }
        }

        return true;
    }

    bool __internal::client::sendhead(Email &msg, int64_t timeout) {
        return sendaddresses(msg.receipts, timeout) &&
               sendaddresses(msg.ccs, timeout) &&
               sendaddresses(msg.bccs, timeout);
    }

    void __internal::client::send(const Email::Address &from, Email &msg, int64_t timeout) {
        int code{0};
        // send from
        if (!sendline(timeout, "MAIL FROM: <", from.email, ">")) {
            throw Exception::create("writing 'MAIL FROM: <", from.email, ">' failed");
        }
        if ((code = getresponse(timeout)) != 250)
        {
            throw Exception::create("'MAIL FROM: <", from.email, ">' failure: ", showerror(code));
        }

        if (!sendhead(msg, timeout))
        {
            throw Exception::create("writing 'MAIL TO: <addresses>' failed.");
        }

        // Send data
        if (!sendline(timeout, "DATA"))
        {
            throw Exception::create("writing DATA to server failed");
        }
        if ((code = getresponse(timeout)) != 354)
        {
            throw Exception::create(
                    "Server rejected 'DATA' command: ", showerror(code));
        }

        String headers = msg.gethead(from);
        if (sock.send(headers, timeout) != headers.size())
        {
            throw Exception::create("sending email header failed: ", errno_s);
        }

        if (!msg.attachments.empty())
        {
            OBuffer b(63);
            b << "--" << msg.boundry << CRLF
              << "Content-Type: " << msg.body_type << "; charset=utf8" CRLF
              << "Content-Encoding: 8bit" CRLF CRLF;
            if (sock.send(b.data(), b.size(), timeout) == 0) {
                throw Exception::create("sending multipart email header failed: ", errno_s);
            }
        }
        // append message
        if (sock.send(msg.bodybuf.data(), msg.bodybuf.size(), timeout) == 0)
        {
            throw Exception::create("sending email message body failed: ", errno_s);
        }

        // Send attachments
        if (!msg.attachments.empty())
        {
            OBuffer b(63);

            b << "\r\n\r\n";
            for (auto &it: msg.attachments)
            {
                auto &at = it.second;
                b << "--" << msg.boundry << CRLF
                  << "Content-Type: " << at.mimetype << "; name=\"" << at.filename() << "\"" CRLF
                  << "Content-Disposition: attachment; filename=\"" << at.filename() << "\"" CRLF
                  << "Content-Transfer-Encoding: 8bit" CRLF CRLF;
                // append message
                if (sock.send(b.data(), b.size(), timeout) == 0) {
                    throw Exception::create("sending attachment '", at.filename(), "' failed: ", errno_s);
                }
                at.send(sock, msg.boundry, timeout);
                b.clear();
                b << CRLF CRLF;
            }
            // send end of boundary
            b << "--" << msg.boundry << CRLF;
            if (sock.send(b.data(), b.size(), timeout) == 0) {
                throw Exception::create("sending multipart closing failed: ", errno_s);
            }
        }

        // send end of data token \r\n.\r\n
        if (!sendline(timeout, CRLF ".")) {
            throw Exception::create(L"sending end of data token '" CRLF "." CRLF "' ", errno_s);
        }

        if ((code = getresponse(timeout)) != 250) {
            showerror(code);
            throw Exception::create("sending email message data failed: ", code);
        }
    }


#undef CRLF
}