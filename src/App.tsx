import { createContext, useContext, useState, useEffect, ReactNode } from "react";
import { Toaster } from "@/components/ui/toaster";
import { Toaster as Sonner } from "@/components/ui/sonner";
import { TooltipProvider } from "@/components/ui/tooltip";
import { QueryClient, QueryClientProvider } from "@tanstack/react-query";
import { BrowserRouter, Routes, Route, useNavigate, Link } from "react-router-dom";
import { useToast } from "@/hooks/use-toast";
import { Button } from "@/components/ui/button";
import { Input } from "@/components/ui/input";
import { Card, CardContent, CardHeader, CardTitle } from "@/components/ui/card";
import { Table, TableBody, TableCell, TableHead, TableHeader, TableRow } from "@/components/ui/table";
import { Eye, EyeOff, TrendingUp, TrendingDown, DollarSign, BarChart3, ArrowUpRight, ArrowDownRight, LogOut, User, Plus, Minus } from "lucide-react";
import { PieChart, Pie, Cell, AreaChart, Area, XAxis, YAxis, CartesianGrid, Tooltip, ResponsiveContainer } from "recharts";
import { api } from "@/lib/api";
import { useAssets, useTransactions, useChartData, useAllocation } from "@/hooks/usePortfolio";
import { TradeModal } from "@/components/TradeModal";
import type { User as UserType } from "@/lib/types";
// ==================== AUTH CONTEXT ====================
interface AuthContextType { 
  user: UserType | null; 
  isAuthenticated: boolean; 
  isLoading: boolean; 
  login: (email: string, password: string) => Promise<void>; 
  signup: (email: string, password: string, username: string) => Promise<void>; 
  logout: () => void; 
}
const AuthContext = createContext<AuthContextType | undefined>(undefined);

function AuthProvider({ children }: { children: ReactNode }) {
  const [user, setUser] = useState<UserType | null>(null);
  const [isLoading, setIsLoading] = useState(true);
  const [useLocalAuth, setUseLocalAuth] = useState(false);

  useEffect(() => {
    // Try to restore session
    const stored = localStorage.getItem("cyber_session");
    if (stored && stored !== "undefined") {
      try {
        setUser(JSON.parse(stored));
      } catch (e) {
        localStorage.removeItem("cyber_session");
      }
    }
    // Check if we should use local auth (backend unavailable)
    api.getMe().then(u => {
      setUser(u);
      localStorage.setItem("cyber_session", JSON.stringify(u));
    }).catch(() => {
      setUseLocalAuth(true);
    }).finally(() => {
      setIsLoading(false);
    });
  }, []);

  const login = async (email: string, password: string) => {
    setIsLoading(true);
    try {
      if (!useLocalAuth) {
        const user = await api.login(email, password);
        setUser(user);
        localStorage.setItem("cyber_session", JSON.stringify(user));
      } else {
        // Fallback to localStorage auth
        await new Promise(r => setTimeout(r, 500));
        const users = JSON.parse(localStorage.getItem("cyber_users") || "[]");
        const found = users.find((u: any) => u.email === email && u.password === password);
        if (!found) throw new Error("Invalid credentials");
        const userData = { id: found.id, email: found.email, username: found.username };
        setUser(userData);
        localStorage.setItem("cyber_session", JSON.stringify(userData));
      }
    } finally {
      setIsLoading(false);
    }
  };

  const signup = async (email: string, password: string, username: string) => {
    setIsLoading(true);
    try {
      if (!useLocalAuth) {
        const user = await api.signup(email, password, username);
        setUser(user);
        localStorage.setItem("cyber_session", JSON.stringify(user));
      } else {
        // Fallback to localStorage auth
        await new Promise(r => setTimeout(r, 500));
        const users = JSON.parse(localStorage.getItem("cyber_users") || "[]");
        if (users.find((u: any) => u.email === email)) throw new Error("Email already exists");
        const newUser = { id: crypto.randomUUID(), email, password, username, createdAt: new Date().toISOString() };
        users.push(newUser);
        localStorage.setItem("cyber_users", JSON.stringify(users));
        const userData = { id: newUser.id, email, username };
        setUser(userData);
        localStorage.setItem("cyber_session", JSON.stringify(userData));
      }
    } finally {
      setIsLoading(false);
    }
  };

  const logout = () => { 
    setUser(null); 
    localStorage.removeItem("cyber_session");
    api.logout();
  };

  return <AuthContext.Provider value={{ user, isAuthenticated: !!user, isLoading, login, signup, logout }}>{children}</AuthContext.Provider>;
}

function useAuth() {
  const ctx = useContext(AuthContext);
  if (!ctx) throw new Error("useAuth must be used within AuthProvider");
  return ctx;
}

// Mock data moved to usePortfolio hook for fallback

// ==================== AUTH PAGE ====================
function AuthPage() {
  const [isLogin, setIsLogin] = useState(true);
  const [email, setEmail] = useState("");
  const [password, setPassword] = useState("");
  const [username, setUsername] = useState("");
  const [showPassword, setShowPassword] = useState(false);
  const [error, setError] = useState("");
  const { login, signup, isAuthenticated, isLoading } = useAuth();
  const { toast } = useToast();
  const navigate = useNavigate();

  useEffect(() => { if (isAuthenticated) navigate("/dashboard"); }, [isAuthenticated, navigate]);

  const handleSubmit = async (e: React.FormEvent) => {
    e.preventDefault();
    setError("");
    try {
      if (isLogin) await login(email, password);
      else await signup(email, password, username);
      toast({ title: isLogin ? "Login successful" : "Account created", description: "Welcome to CyberFinance" });
    } catch (err: any) {
      setError(err.message);
      toast({ title: "Error", description: err.message, variant: "destructive" });
    }
  };

  return (
    <div className="min-h-screen bg-background flex items-center justify-center p-4">
      <div className="absolute inset-0 cyber-grid opacity-20" />
      <Card className="w-full max-w-md cyber-card relative z-10">
        <CardHeader className="text-center">
          <CardTitle className="text-3xl font-display text-primary neon-text">CYBERFINANCE</CardTitle>
          <p className="text-muted-foreground">Secure Financial Terminal</p>
        </CardHeader>
        <CardContent>
          <div className="flex mb-6">
            <button onClick={() => setIsLogin(true)} className={`flex-1 py-2 text-center transition-all ${isLogin ? "text-primary border-b-2 border-primary" : "text-muted-foreground"}`}>LOGIN</button>
            <button onClick={() => setIsLogin(false)} className={`flex-1 py-2 text-center transition-all ${!isLogin ? "text-primary border-b-2 border-primary" : "text-muted-foreground"}`}>REGISTER</button>
          </div>
          <form onSubmit={handleSubmit} className="space-y-4">
            {!isLogin && <Input placeholder="Username" value={username} onChange={e => setUsername(e.target.value)} className="cyber-input" required />}
            <Input type="email" placeholder="Email" value={email} onChange={e => setEmail(e.target.value)} className="cyber-input" required />
            <div className="relative">
              <Input type={showPassword ? "text" : "password"} placeholder="Password" value={password} onChange={e => setPassword(e.target.value)} className="cyber-input pr-10" required />
              <button type="button" onClick={() => setShowPassword(!showPassword)} className="absolute right-3 top-1/2 -translate-y-1/2 text-muted-foreground">
                {showPassword ? <EyeOff size={18} /> : <Eye size={18} />}
              </button>
            </div>
            {error && <p className="text-destructive text-sm">{error}</p>}
            <Button type="submit" className="w-full cyber-button" disabled={isLoading}>{isLoading ? "PROCESSING..." : isLogin ? "ACCESS SYSTEM" : "CREATE ACCOUNT"}</Button>
          </form>
        </CardContent>
      </Card>
    </div>
  );
}

// ==================== DASHBOARD PAGE ====================
function DashboardPage() {
  const { user, isAuthenticated, logout } = useAuth();
  const navigate = useNavigate();
  const { data: assets = [], isLoading: assetsLoading } = useAssets();
  const { data: transactions = [], isLoading: txLoading } = useTransactions();
  const { data: chartData = [] } = useChartData();
  const { data: allocation = [] } = useAllocation();
  const [tradeModalOpen, setTradeModalOpen] = useState(false);
  const [tradeType, setTradeType] = useState<"buy" | "sell">("buy");

  useEffect(() => { if (!isAuthenticated) navigate("/auth"); }, [isAuthenticated, navigate]);
  if (!isAuthenticated) return null;

  const openTradeModal = (type: "buy" | "sell") => {
    setTradeType(type);
    setTradeModalOpen(true);
  };

  const formatCurrency = (v: number) => new Intl.NumberFormat("en-US", { style: "currency", currency: "USD" }).format(v);
  const totalValue = assets.reduce((sum, a) => sum + a.quantity * a.current_price, 0);
  const totalCost = assets.reduce((sum, a) => sum + a.quantity * a.avg_cost, 0);
  const totalGain = totalValue - totalCost;
  const gainPercent = totalCost > 0 ? ((totalGain / totalCost) * 100).toFixed(2) : "0.00";

  if (assetsLoading) {
    return (
      <div className="min-h-screen bg-background flex items-center justify-center">
        <div className="text-primary text-xl">Loading portfolio...</div>
      </div>
    );
  }

  return (
    <div className="min-h-screen bg-background">
      <div className="absolute inset-0 cyber-grid opacity-10" />
      {/* Header */}
      <header className="border-b border-border/50 bg-card/80 backdrop-blur-sm sticky top-0 z-50">
        <div className="flex items-center justify-between p-4 max-w-7xl mx-auto">
          <h1 className="text-xl font-display text-primary neon-text">CYBERFINANCE</h1>
          <div className="flex items-center gap-4">
            <Button size="sm" onClick={() => openTradeModal("buy")} className="bg-green-600 hover:bg-green-700 text-white">
              <Plus size={16} /> Buy
            </Button>
            <Button size="sm" onClick={() => openTradeModal("sell")} className="bg-red-600 hover:bg-red-700 text-white">
              <Minus size={16} /> Sell
            </Button>
            <span className="text-muted-foreground flex items-center gap-2"><User size={16} />{user?.username}</span>
            <Button variant="outline" size="sm" onClick={async () => {await logout(); navigate("/");
             }}className="text-destructive"
            ><LogOut size={16} /> </Button>
          </div>
        </div>
      </header>
      
      <TradeModal open={tradeModalOpen} onOpenChange={setTradeModalOpen} type={tradeType} assets={assets} />

      <main className="p-4 max-w-7xl mx-auto relative z-10 space-y-6">
        {/* Stats */}
        <div className="grid grid-cols-1 md:grid-cols-4 gap-4">
          <Card className="cyber-card"><CardContent className="p-4"><div className="flex items-center justify-between"><div className="text-muted-foreground text-sm">Total Value</div><DollarSign className="text-primary" size={20} /></div><div className="text-2xl font-bold text-foreground truncate">{formatCurrency(totalValue)}</div></CardContent></Card>
          <Card className="cyber-card"><CardContent className="p-4"><div className="flex items-center justify-between"><div className="text-muted-foreground text-sm">Total Gain</div>{totalGain >= 0 ? <TrendingUp className="text-green-400" size={20} /> : <TrendingDown className="text-red-400" size={20} />}</div><div className={`text-2xl font-bold truncate ${totalGain >= 0 ? "text-green-400" : "text-red-400"}`}>{formatCurrency(totalGain)} ({gainPercent}%)</div></CardContent></Card>
          <Card className="cyber-card"><CardContent className="p-4"><div className="flex items-center justify-between"><div className="text-muted-foreground text-sm">Assets</div><BarChart3 className="text-accent" size={20} /></div><div className="text-2xl font-bold text-foreground">{assets.length}</div></CardContent></Card>
          <Card className="cyber-card"><CardContent className="p-4"><div className="flex items-center justify-between"><div className="text-muted-foreground text-sm">Day Change</div><ArrowUpRight className="text-green-400" size={20} /></div><div className="text-2xl font-bold text-green-400">+2.34%</div></CardContent></Card>
        </div>

        {/* Charts */}
        <div className="grid grid-cols-1 lg:grid-cols-3 gap-4">
          <Card className="cyber-card lg:col-span-2"><CardHeader><CardTitle className="text-primary">Portfolio Performance</CardTitle></CardHeader><CardContent className="h-64">
            <ResponsiveContainer width="100%" height="100%"><AreaChart data={chartData}><defs><linearGradient id="colorValue" x1="0" y1="0" x2="0" y2="1"><stop offset="5%" stopColor="hsl(var(--primary))" stopOpacity={0.3}/><stop offset="95%" stopColor="hsl(var(--primary))" stopOpacity={0}/></linearGradient></defs><CartesianGrid strokeDasharray="3 3" stroke="hsl(var(--border))" /><XAxis dataKey="date" stroke="hsl(var(--muted-foreground))" /><YAxis stroke="hsl(var(--muted-foreground))" /><Tooltip contentStyle={{ backgroundColor: "hsl(var(--card))", border: "1px solid hsl(var(--border))" }} /><Area type="monotone" dataKey="value" stroke="hsl(var(--primary))" fillOpacity={1} fill="url(#colorValue)" /></AreaChart></ResponsiveContainer>
          </CardContent></Card>
          <Card className="cyber-card"><CardHeader><CardTitle className="text-primary">Allocation</CardTitle></CardHeader><CardContent className="h-64">
            <ResponsiveContainer width="100%" height="100%"><PieChart><Pie data={allocation} cx="50%" cy="50%" innerRadius={50} outerRadius={80} dataKey="value" label={({ name, value }) => `${name}: ${value}%`}>{allocation.map((entry, i) => <Cell key={i} fill={entry.color} />)}</Pie><Tooltip /></PieChart></ResponsiveContainer>
          </CardContent></Card>
        </div>

        {/* Assets Table */}
        <Card className="cyber-card"><CardHeader><CardTitle className="text-primary">Assets</CardTitle></CardHeader><CardContent>
          <Table><TableHeader><TableRow><TableHead>Symbol</TableHead><TableHead>Name</TableHead><TableHead>Quantity</TableHead><TableHead>Price</TableHead><TableHead>Value</TableHead><TableHead>P/L</TableHead></TableRow></TableHeader>
            <TableBody>{assets.map(a => { const value = a.quantity * a.current_price; const pl = value - a.quantity * a.avg_cost; return (
              <TableRow key={a.id}><TableCell className="font-bold text-primary">{a.symbol}</TableCell><TableCell>{a.name}</TableCell><TableCell>{a.quantity}</TableCell><TableCell>{formatCurrency(a.current_price)}</TableCell><TableCell>{formatCurrency(value)}</TableCell><TableCell className={pl >= 0 ? "text-green-400" : "text-red-400"}>{pl >= 0 ? <ArrowUpRight className="inline" size={14} /> : <ArrowDownRight className="inline" size={14} />} {formatCurrency(pl)}</TableCell></TableRow>
            )})}</TableBody></Table>
        </CardContent></Card>

        {/* Transactions */}
        <Card className="cyber-card"><CardHeader><CardTitle className="text-primary">Recent Transactions</CardTitle></CardHeader><CardContent>
          <div className="space-y-3">{transactions.map(t => (
            <div key={t.id} className="flex items-center justify-between p-3 rounded border border-border/50 bg-card/50">
              <div className="flex items-center gap-3"><span className={`px-2 py-1 text-xs rounded ${t.type === "buy" ? "bg-green-500/20 text-green-400" : "bg-red-500/20 text-red-400"}`}>{t.type.toUpperCase()}</span><span className="font-bold">{t.asset}</span></div>
              <div className="text-right"><div className="font-mono">{formatCurrency(t.total)}</div><div className="text-xs text-muted-foreground">{new Date(t.timestamp).toLocaleDateString()}</div></div>
            </div>
          ))}</div>
        </CardContent></Card>
      </main>
    </div>
  );
}

// ==================== LANDING PAGE ====================
function LandingPage() {
  return (
    <div className="min-h-screen bg-background flex flex-col items-center justify-center p-4 text-center">
      <div className="absolute inset-0 cyber-grid opacity-20" />
      <div className="relative z-10 space-y-8">
        <h1 className="text-5xl md:text-7xl font-display text-primary neon-text">CYBERFINANCE</h1>
        <p className="text-xl text-muted-foreground max-w-md mx-auto">Next-generation portfolio tracking for the digital age</p>
        <div className="flex gap-4 justify-center">
          <Link to="/auth"><Button className="cyber-button">Get Started</Button></Link>
          <Link to="/dashboard"><Button variant="outline">View Dashboard</Button></Link>
        </div>
      </div>
    </div>
  );
}

// ==================== APP ====================
const queryClient = new QueryClient();

const App = () => (
  <QueryClientProvider client={queryClient}>
    <AuthProvider>
      <TooltipProvider>
        <Toaster />
        <Sonner />
        <BrowserRouter>
          <Routes>
            <Route path="/" element={<LandingPage />} />
            <Route path="/auth" element={<AuthPage />} />
            <Route path="/dashboard" element={<DashboardPage />} />
            <Route path="*" element={<LandingPage />} />
          </Routes>
        </BrowserRouter>
      </TooltipProvider>
    </AuthProvider>
  </QueryClientProvider>
);

export default App;
