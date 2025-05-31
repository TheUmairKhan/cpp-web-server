# API Documentation

## Overview

This document describes the HTTP API exposed by our C++ web server. It covers all available endpoints, their HTTP methods, expected request parameters or bodies, and response formats. The API follows RESTful principles for CRUD operations under a single `/api` endpoint and provides special handlers for static files, echo functionality, and dynamic Markdown-to-HTML rendering.

---

## Base URL

```
http://<server_host>:<port>/
```

Replace `<server_host>` with your server’s hostname or IP address and `<port>` with the port your server is listening on (e.g., 8080).

---

## Common HTTP Response Codes

* **200 OK**
  Successful GET, PUT, or POST that returns content.
* **201 Created**
  Successful POST that creates a new resource.
* **204 No Content**
  Successful DELETE (no body returned).
* **400 Bad Request**
  Malformed request syntax or invalid parameters.
* **404 Not Found**
  Requested resource does not exist.
* **500 Internal Server Error**
  Unexpected server-side error.

---

## 1. Static File Handler

### `GET /static/<filepath>`

* **Description**: Serves a static file from the server’s document root.
* **Request Parameters**

  * `<filepath>` (path segment): Relative path to a file under the configured document root.
* **Response**

  * **200 OK**: Returns the raw file bytes.

    * `Content-Type` header is set based on file extension (e.g., `text/html`, `image/png`, `application/javascript`, etc.).
  * **404 Not Found**: If `<filepath>` does not exist or is outside the document root.

#### Example

```
GET /static/css/styles.css HTTP/1.1
Host: example.com:8080
```

* Response (successful):

  ```
  HTTP/1.1 200 OK
  Content-Type: text/css
  Content-Length: 1423

  /* contents of styles.css ... */
  ```

---

## 2. Echo Handler

### `GET /echo`

* **Description**: Echos back a message provided as a query parameter.
* **Query Parameters**

  * `message` (string, required): The text to echo back.
* **Response**

  * **200 OK**: Returns a JSON object containing the echoed message.

    ```json
    {
      "echo": "<message>"
    }
    ```
  * **400 Bad Request**: If the `message` parameter is missing or empty.

#### Example

```
GET /echo?message=hello%20world HTTP/1.1
Host: example.com:8080
```

* Response:

  ```json
  HTTP/1.1 200 OK
  Content-Type: application/json
  Content-Length: 28

  {
    "echo": "hello world"
  }
  ```

---

## 3. CRUD Handler (JSON-based Data Storage)

This handler manages resources of type `item`. All data is stored in a server-side JSON file (e.g., `items.json`). All CRUD operations use the `/api` base path.

### Base Path

```
/api
```

### 3.1. Retrieve All Items

#### `GET /api`

* **Description**: Fetches a list of all items.
* **Response**

  * **200 OK**: Returns an array of item objects.

    ```json
    [
      {
        "id": 1,
        "name": "Widget A",
        "quantity": 10,
        "price": 19.99
      },
      {
        "id": 2,
        "name": "Widget B",
        "quantity": 5,
        "price": 29.99
      }
      // …
    ]
    ```
  * **500 Internal Server Error**: If the JSON file is unreadable or corrupted.

#### Example

```
GET /api HTTP/1.1
Host: example.com:8080
Accept: application/json
```

---

### 3.2. Retrieve a Single Item

#### `GET /api/{id}`

* **Description**: Fetches a single item by its unique `id`.
* **Path Parameters**

  * `{id}` (integer, required): Unique identifier of the item.
* **Response**

  * **200 OK**: Returns a JSON object for the requested item.

    ```json
    {
      "id": 2,
      "name": "Widget B",
      "quantity": 5,
      "price": 29.99
    }
    ```
  * **404 Not Found**: If no item with the specified `id` exists.
  * **400 Bad Request**: If `{id}` is not a valid integer.

#### Example

```
GET /api/2 HTTP/1.1
Host: example.com:8080
Accept: application/json
```

---

### 3.3. Create a New Item

#### `POST /api`

* **Description**: Adds a new item to the collection.
* **Request Body** (JSON)

  * `name` (string, required): Name of the item.
  * `quantity` (integer, required): Number of units in stock.
  * `price` (number, required): Unit price (float).
* **Response**

  * **201 Created**: Returns the newly created item (including its assigned `id`).

    ```json
    {
      "id": 3,
      "name": "Widget C",
      "quantity": 20,
      "price": 9.99
    }
    ```
  * **400 Bad Request**: If any required field is missing or invalid (e.g., negative quantity, non-numeric price).

#### Example

```
POST /api HTTP/1.1
Host: example.com:8080
Content-Type: application/json

{
  "name": "Widget C",
  "quantity": 20,
  "price": 9.99
}
```

* Response (successful):

  ```json
  HTTP/1.1 201 Created
  Content-Type: application/json
  Content-Length: 72

  {
    "id": 3,
    "name": "Widget C",
    "quantity": 20,
    "price": 9.99
  }
  ```

---

### 3.4. Update an Existing Item

#### `PUT /api/{id}`

* **Description**: Updates fields of an existing item identified by `id`.
* **Path Parameters**

  * `{id}` (integer, required): Unique identifier of the item to update.
* **Request Body** (JSON)

  * Can include one or more of the following fields to modify:

    * `name` (string, optional)
    * `quantity` (integer, optional)
    * `price` (number, optional)
* **Response**

  * **200 OK**: Returns the updated item object.

    ```json
    {
      "id": 2,
      "name": "Widget B",
      "quantity": 8,
      "price": 29.99
    }
    ```
  * **400 Bad Request**: If any provided field is invalid (e.g., non-numeric `quantity`).
  * **404 Not Found**: If no item with the specified `id` exists.

#### Example

```
PUT /api/2 HTTP/1.1
Host: example.com:8080
Content-Type: application/json

{
  "quantity": 8
}
```

* Response (successful):

  ```json
  HTTP/1.1 200 OK
  Content-Type: application/json
  Content-Length: 64

  {
    "id": 2,
    "name": "Widget B",
    "quantity": 8,
    "price": 29.99
  }
  ```

---

### 3.5. Delete an Item

#### `DELETE /api/{id}`

* **Description**: Removes an item from the collection.
* **Path Parameters**

  * `{id}` (integer, required): Unique identifier of the item to delete.
* **Response**

  * **204 No Content**: Indicates successful deletion; no response body.
  * **404 Not Found**: If no item with the specified `id` exists.
  * **400 Bad Request**: If `{id}` is not a valid integer.

#### Example

```
DELETE /api/2 HTTP/1.1
Host: example.com:8080
```

* Response (successful):

  ```
  HTTP/1.1 204 No Content
  ```

---

## 4. Markdown Handler

### `GET /docs/<filename>.md`

* **Description**: Reads a Markdown file from the server’s `docs/` directory, converts it to HTML on-the-fly, and returns the rendered HTML to the client.
* **Request Parameters**

  * `<filename>.md` (path segment, required): Name of a Markdown file located under the `docs/` folder (e.g., `getting-started.md`).
* **Response**

  * **200 OK**: Returns an HTML document generated from the requested Markdown file.

    * `Content-Type: text/html; charset=utf-8`
  * **404 Not Found**: If the specified Markdown file does not exist.

#### Example

```
GET /docs/getting-started.md HTTP/1.1
Host: example.com:8080
Accept: text/html
```

* Response (successful):

  ```
  HTTP/1.1 200 OK
  Content-Type: text/html; charset=utf-8
  Content-Length: 4523

  <!DOCTYPE html>
  <html>
    <head>
      <meta charset="utf-8" />
      <title>Getting Started</title>
    </head>
    <body>
      <h1>Getting Started</h1>
      <p>Welcome to our project.documentation...</p>
      <!-- Converted HTML from Markdown -->
    </body>
  </html>
  ```

---

## 5. Health Check Endpoint

### `GET /health`

* **Description**: Simple endpoint to verify that the server is running.
* **Response**

  * **200 OK**: Returns a plain-text response body "OK".

#### Example

```
GET /health HTTP/1.1
Host: example.com:8080
```

* Response:

  ```
  HTTP/1.1 200 OK
  Content-Type: text/plain
  Content-Length: 2

  OK
  ```

---

## 6. Error Response Format

When an error occurs (4xx or 5xx), the server returns a JSON object with the following structure:

```json
{
  "error": "<HTTP status code description>",
  "message": "<detailed explanation of what went wrong>"
}
```

* **error**: A short string describing the HTTP status (e.g., "Bad Request", "Not Found", "Internal Server Error").
* **message**: Detailed information about why the request failed (e.g., missing parameters, file not found, JSON parse error).

### Example: 400 Bad Request

```
HTTP/1.1 400 Bad Request
Content-Type: application/json
Content-Length: 78

{
  "error": "Bad Request",
  "message": "Field 'name' is required and must be a non-empty string."
}
```

---

## 7. Example Usage with `curl`

1. **Fetch All Items**

   ```
   curl -i http://example.com:8080/api
   ```

2. **Create a New Item**

   ```
   curl -i -X POST http://example.com:8080/api \
     -H "Content-Type: application/json" \
     -d '{"name":"Widget D","quantity":15,"price":14.95}'
   ```

3. **Update an Item**

   ```
   curl -i -X PUT http://example.com:8080/api/3 \
     -H "Content-Type: application/json" \
     -d '{"price":12.49}'
   ```

4. **Delete an Item**

   ```
   curl -i -X DELETE http://example.com:8080/api/3
   ```

5. **Render a Markdown File**

   ```
   curl -i http://example.com:8080/docs/overview.md
   ```

6. **Echo a Message**

   ```
   curl -i http://example.com:8080/echo?message=testing123
   ```

7. **Check Server Health**

   ```
   curl -i http://example.com:8080/health
   ```

---

## 8. Notes & Best Practices

* All endpoints return JSON except the static, markdown, and health-check routes (which return raw file bytes, HTML, or plain text, respectively).
* The CRUD handler ensures that each new item receives a unique integer `id`. It persists data in a JSON file on disk, so multiple server restarts will preserve existing items.
* Input validation is performed on all POST and PUT requests. Invalid or missing fields will result in a 400 response with a JSON error.
* The Markdown handler sanitizes HTML output to prevent XSS attacks. Only basic Markdown syntax is supported (headers, lists, code blocks, inline formatting, links, images).
* For any unrecognized path or HTTP method, the server responds with a 404 status.

---

*End of `api.md`*
