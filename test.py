from cquery import crud

host = "localhost"
user = "root"
password = "password"
database = "test_db"


# Define a model
user_model = {"users": {"id": "INT AUTO_INCREMENT PRIMARY KEY", "name": "VARCHAR(255)", "age": "INT"}}

# Create the table
crud.create_table(host, user, password, database, user_model)

# Insert a record
record = {"id": None, "name": "Alice", "age": 30}
crud.insert(host, user, password, database, "users", record)

# Read all records
records = crud.read_all(host, user, password, database, "users")
print(records)