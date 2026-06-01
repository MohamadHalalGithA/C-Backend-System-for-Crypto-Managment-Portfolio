

import type {
  Asset,
  Transaction,
  ChartDataPoint,
  AllocationData,
  User,
  AuthResponse,
  ApiResponse,
} from "./types";

const API_URL = "http://localhost:8080/api";

class ApiClient {
  private async request<T>(endpoint: string, options: RequestInit = {}): Promise<T> {
    const headers: Record<string, string> = {
      "Content-Type": "application/json",
      ...(options.headers as Record<string, string>),
    };

    try {
      const response = await fetch(`${API_URL}${endpoint}`, {
        ...options,
        headers,
        // ✅ IMPORTANT: send/receive cookies (session=...)
        credentials: "include",
      });

      // Some responses might be empty (204, etc.)
      const text = await response.text();
      const data = text ? JSON.parse(text) : {};

      if (!response.ok) {
        throw new Error(data.error || `HTTP ${response.status}`);
      }

      return data as T;
    } catch (error) {
      if (error instanceof TypeError && error.message.includes("fetch")) {
        throw new Error("Backend unavailable. Ensure C server is running on port 8080.");
      }
      throw error;
    }
  }

  // ✅ Auth endpoints (match your C server routes)
  async login(email: string, password: string): Promise<User> {
    // backend route: POST /api/login
    const response = await this.request<ApiResponse<User>>("/login", {
      method: "POST",
      body: JSON.stringify({ email, password }),
    });

    if (response.success && response.data) return response.data;
    throw new Error(response.error || "Login failed");
  }

  async signup(email: string, password: string, username: string): Promise<User> {
    // backend route: POST /api/signup
    const response = await this.request<ApiResponse<User>>("/signup", {
      method: "POST",
      body: JSON.stringify({ email, password, username }),
    });

    if (response.success && response.data) return response.data;
    throw new Error(response.error || "Signup failed");
  }

  async logout(): Promise<void> {
    // backend route: POST /api/logout
    // clears cookie in backend
    try {
      await this.request<ApiResponse<{}>>("/logout", { method: "POST" });
    } finally {
      // nothing stored locally for cookie auth, but keep it safe
      localStorage.removeItem("cyber_token");
    }
  }

  async getMe(): Promise<User> {
    // backend route: GET /api/me
    const response = await this.request<ApiResponse<User>>("/me");
    if (response.success && response.data) return response.data;
    throw new Error(response.error || "Failed to get user");
  }

  // Assets endpoints
  async getAssets(): Promise<Asset[]> {
    const response = await this.request<ApiResponse<Asset[]>>("/assets");
    if (response.success && response.data) return response.data;
    throw new Error(response.error || "Failed to fetch assets");
  }

  async createAsset(
    asset: Omit<Asset, "id" | "user_id" | "created_at" | "updated_at">
  ): Promise<Asset> {
    const response = await this.request<ApiResponse<Asset>>("/assets", {
      method: "POST",
      body: JSON.stringify(asset),
    });
    if (response.success && response.data) return response.data;
    throw new Error(response.error || "Failed to create asset");
  }

  async updateAsset(id: string, asset: Partial<Asset>): Promise<Asset> {
    const response = await this.request<ApiResponse<Asset>>(`/assets/${id}`, {
      method: "PUT",
      body: JSON.stringify(asset),
    });
    if (response.success && response.data) return response.data;
    throw new Error(response.error || "Failed to update asset");
  }

  async deleteAsset(id: string): Promise<void> {
    const response = await this.request<ApiResponse<void>>(`/assets/${id}`, {
      method: "DELETE",
    });
    if (!response.success) throw new Error(response.error || "Failed to delete asset");
  }

  // Transactions endpoints
  async getTransactions(): Promise<Transaction[]> {
    const response = await this.request<ApiResponse<Transaction[]>>("/transactions");
    if (response.success && response.data) return response.data;
    throw new Error(response.error || "Failed to fetch transactions");
  }

  async createTransaction(tx: Omit<Transaction, "id" | "user_id">): Promise<Transaction> {
    const response = await this.request<ApiResponse<Transaction>>("/transactions", {
      method: "POST",
      body: JSON.stringify(tx),
    });
    if (response.success && response.data) return response.data;
    throw new Error(response.error || "Failed to create transaction");
  }

  // Portfolio data endpoints
  async getChartData(): Promise<ChartDataPoint[]> {
    const response = await this.request<ApiResponse<ChartDataPoint[]>>("/portfolio/chart");
    if (response.success && response.data) return response.data;
    throw new Error(response.error || "Failed to fetch chart data");
  }

  async getAllocation(): Promise<AllocationData[]> {
    const response = await this.request<ApiResponse<AllocationData[]>>("/portfolio/allocation");
    if (response.success && response.data) return response.data;
    throw new Error(response.error || "Failed to fetch allocation");
  }
}

export const api = new ApiClient();
