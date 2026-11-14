#pragma once

#include <map>
#include <string>

namespace lms {

// A login account and its assigned authorization role.
struct User {
    int id{};
    std::string username;
    std::string salt;
    std::string passwordHash;
    std::string role;
    bool active{true};
};

// Inventory record: available copies cannot exceed total copies.
struct Book {
    int id{};
    std::string title;
    std::string author;
    std::string category;
    int total{};
    int available{};
};

// Approved book borrowing record, including issue and return dates.
struct Loan {
    int id{};
    int bookId{};
    int memberId{};
    std::string issued;
    std::string due;
    std::string returned;
};

// Member's request, pending approval or rejection by staff.
struct Request {
    int id{};
    int bookId{};
    int memberId{};
    std::string status{"PENDING"};
};

// Application logic and persistent record collections.
class Library {
public:
    // File-based persistence.
    bool load(const std::string& path);
    bool save(const std::string& path) const;

    bool initialized() const { return !users.empty(); }

    // Account authentication and administrator-only account management.
    bool createAdmin(const std::string& username, const std::string& password);
    const User* login(const std::string& username, const std::string& password) const;
    bool addUser(const User& actor, const std::string& username,
                 const std::string& password, const std::string& role);
    bool setUserActive(const User& actor, int userId, bool active);

    // Book inventory operations.
    bool addBook(const User& actor, const Book& book);
    bool updateBook(const User& actor, const Book& book);
    bool deleteBook(const User& actor, int bookId);

    // Request approval, book issue, and return operations.
    bool requestBook(const User& actor, int bookId);
    bool approveRequest(const User& actor, int requestId,
                        const std::string& issued, const std::string& due);
    bool rejectRequest(const User& actor, int requestId);
    bool returnBook(const User& actor, int loanId,
                    const std::string& returned, int& fine);

    // Read-only access to stored records for console display.
    const std::map<int, User>& allUsers() const { return users; }
    const std::map<int, Book>& allBooks() const { return books; }
    const std::map<int, Request>& allRequests() const { return requests; }
    const std::map<int, Loan>& allLoans() const { return loans; }

    static bool validDate(const std::string& date);
    static int overdueFine(const std::string& due, const std::string& returned);

private:
    std::map<int, User> users;
    std::map<int, Book> books;
    std::map<int, Request> requests;
    std::map<int, Loan> loans;

    int nextUser{1};
    int nextRequest{1};
    int nextLoan{1};
};

// Password derivation and salt generation (implemented in security.cpp).
std::string hashPassword(const std::string& password, const std::string& salt);
std::string newSalt();

}  // namespace lms
